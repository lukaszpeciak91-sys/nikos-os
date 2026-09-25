#include "battery_guard/battery_guard.hpp"
#include "board/board.hpp"
#include "clock/clock_service.hpp"
#include "communicator/communicator_app.hpp"
#include "countdown/countdown_service.hpp"
#include "launcher/launcher.hpp"
#include "messaging/messaging_service.hpp"
#include "power/display_lifecycle.hpp"
#include "power_diag/power_diag_app.hpp"
#include "power_diag/power_diag_session.hpp"
#include "radiolab/radiolab_app.hpp"
#include "radio/radio.hpp"
#include "settings/settings.hpp"
#include "signal_sound/signal_sound_player.hpp"

#include "esp_err.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "nvs_flash.h"

namespace {

constexpr char kTag[] = "main";
constexpr std::uint8_t kRadioChannel = 6;
constexpr std::uint32_t kSplashDurationMs = 750;
constexpr std::uint32_t kLoopDelayMs = 20;
constexpr std::uint32_t kRadioErrorDisplayMs = 1200;
constexpr std::uint32_t kClockGlanceDurationMs = 4000;
constexpr std::uint32_t kCriticalShutdownMessageMs = 1750;
constexpr std::uint32_t kChargingVbusPollIntervalMs = 1000;
constexpr std::int16_t kChargingVbusPresentMv = 4000;
constexpr std::int16_t kChargingFullBatteryMv = 4100;
constexpr std::uint8_t kChargingFullConfirmSamples = 3;
constexpr std::uint32_t kChargingPresentationMs = 5000;
constexpr std::uint32_t kChargingAnimationStepMs = 400;
constexpr float kChargingFullToneHz = 3200.0F;
constexpr std::uint32_t kChargingFullToneMs = 90;

enum class RuntimeState : std::uint8_t {
    Launcher,
    Communicator,
    PowerDiag,
    RadioLab,
};

enum class ChargingCableEvent : std::uint8_t {
    None,
    Connected,
    Disconnected,
};

struct ChargingModeState {
    bool active = false;
    bool full_latched = false;
    bool charging_observed = false;
    std::uint8_t full_confirm_count = 0;

    bool presentation_visible = false;
    std::uint8_t animation_step = 0;
    std::uint32_t presentation_started_ms = 0;
    std::uint32_t last_animation_ms = 0;

    bool vbus_sample_valid = false;
    std::uint32_t last_vbus_sample_ms = 0;
};

std::uint64_t monotonic_now_us()
{
    return static_cast<std::uint64_t>(esp_timer_get_time());
}

std::uint32_t now_ms()
{
    return static_cast<std::uint32_t>(
        pdTICKS_TO_MS(xTaskGetTickCount()));
}

bool any_user_button_activity(const nikos::board::InputState& input)
{
    return input.primary_pressed
        || input.secondary_pressed
        || input.primary_short
        || input.primary_long
        || input.secondary_short
        || input.secondary_long;
}

bool charging_wake_requested(const nikos::board::InputState& input)
{
    return input.primary_short
        || input.secondary_short
        || input.power_short;
}

void reset_charging_session(ChargingModeState& state)
{
    state.active = false;
    state.full_latched = false;
    state.charging_observed = false;
    state.full_confirm_count = 0;
    state.presentation_visible = false;
    state.animation_step = 0;
    state.presentation_started_ms = 0;
    state.last_animation_ms = 0;
}

ChargingCableEvent poll_charging_cable(
    nikos::board::Board& board,
    ChargingModeState& state,
    std::uint32_t now)
{
    if (state.vbus_sample_valid
        && now - state.last_vbus_sample_ms
            < kChargingVbusPollIntervalMs) {
        return ChargingCableEvent::None;
    }

    state.vbus_sample_valid = true;
    state.last_vbus_sample_ms = now;

    const bool vbus_present =
        board.vbus_voltage_mv() >= kChargingVbusPresentMv;

    if (vbus_present && !state.active) {
        state.active = true;
        state.full_latched = false;
        state.charging_observed = false;
        state.full_confirm_count = 0;
        state.presentation_visible = false;
        state.animation_step = 0;
        return ChargingCableEvent::Connected;
    }

    if (!vbus_present && state.active) {
        reset_charging_session(state);
        return ChargingCableEvent::Disconnected;
    }

    return ChargingCableEvent::None;
}

void draw_battery_outline(nikos::board::Board& board)
{
    constexpr std::int16_t x = 42;
    constexpr std::int16_t y = 18;
    constexpr std::int16_t width = 148;
    constexpr std::int16_t height = 64;
    constexpr std::int16_t right =
        static_cast<std::int16_t>(x + width - 1);
    constexpr std::int16_t bottom =
        static_cast<std::int16_t>(y + height - 1);

    for (std::int16_t offset = 0; offset < 2; ++offset) {
        board.draw_line(
            static_cast<std::int16_t>(x + offset),
            static_cast<std::int16_t>(y + offset),
            static_cast<std::int16_t>(right - offset),
            static_cast<std::int16_t>(y + offset),
            nikos::board::DisplayColor::PrimaryText);
        board.draw_line(
            static_cast<std::int16_t>(x + offset),
            static_cast<std::int16_t>(bottom - offset),
            static_cast<std::int16_t>(right - offset),
            static_cast<std::int16_t>(bottom - offset),
            nikos::board::DisplayColor::PrimaryText);
        board.draw_line(
            static_cast<std::int16_t>(x + offset),
            static_cast<std::int16_t>(y + offset),
            static_cast<std::int16_t>(x + offset),
            static_cast<std::int16_t>(bottom - offset),
            nikos::board::DisplayColor::PrimaryText);
        board.draw_line(
            static_cast<std::int16_t>(right - offset),
            static_cast<std::int16_t>(y + offset),
            static_cast<std::int16_t>(right - offset),
            static_cast<std::int16_t>(bottom - offset),
            nikos::board::DisplayColor::PrimaryText);
    }

    board.fill_rect(
        190,
        37,
        9,
        26,
        nikos::board::DisplayColor::PrimaryText);
}

void render_charging_presentation(
    nikos::board::Board& board,
    std::uint8_t animation_step)
{
    constexpr std::int16_t fill_x = 48;
    constexpr std::int16_t fill_y = 24;
    constexpr std::int16_t fill_width = 136;
    constexpr std::int16_t fill_height = 52;

    board.clear_screen();
    draw_battery_outline(board);

    const std::uint8_t step =
        static_cast<std::uint8_t>((animation_step % 4U) + 1U);
    const std::int16_t current_fill_width =
        static_cast<std::int16_t>(
            (fill_width * static_cast<std::int16_t>(step)) / 4);

    board.fill_rect(
        fill_x,
        fill_y,
        current_fill_width,
        fill_height,
        nikos::board::DisplayColor::Accent);

    board.draw_text_region(
        39,
        100,
        190,
        28,
        "LADOWANIE",
        3,
        nikos::board::DisplayColor::PrimaryText,
        nikos::board::DisplayColor::Background);
}

void render_charging_full(nikos::board::Board& board)
{
    board.clear_screen();
    draw_battery_outline(board);
    board.fill_rect(
        48,
        24,
        136,
        52,
        nikos::board::DisplayColor::StatusActive);
    board.draw_text_region(
        90,
        94,
        80,
        36,
        "OK",
        5,
        nikos::board::DisplayColor::StatusActive,
        nikos::board::DisplayColor::Background);
}

void show_charging_presentation(
    nikos::board::Board& board,
    nikos::power::DisplayLifecycle& display_lifecycle,
    ChargingModeState& state,
    std::uint32_t now)
{
    display_lifecycle.note_visible_activity();
    state.presentation_visible = true;
    state.presentation_started_ms = now;
    state.last_animation_ms = now;
    state.animation_step = 0;

    if (state.full_latched) {
        render_charging_full(board);
    } else {
        render_charging_presentation(board, state.animation_step);
    }
}

void update_charging_presentation(
    nikos::board::Board& board,
    nikos::power::DisplayLifecycle& display_lifecycle,
    ChargingModeState& state,
    std::uint32_t now)
{
    if (!state.presentation_visible) {
        return;
    }

    if (now - state.presentation_started_ms >= kChargingPresentationMs) {
        state.presentation_visible = false;
        display_lifecycle.display_off_now();
        return;
    }

    if (state.full_latched
        || now - state.last_animation_ms < kChargingAnimationStepMs) {
        return;
    }

    state.last_animation_ms = now;
    state.animation_step =
        static_cast<std::uint8_t>((state.animation_step + 1U) % 4U);
    render_charging_presentation(board, state.animation_step);
}

bool update_charging_full_confirmation(
    ChargingModeState& state,
    const nikos::battery_guard::UpdateResult& battery_update)
{
    if (!state.active
        || state.full_latched
        || !battery_update.sampled) {
        return false;
    }

    const nikos::board::PowerStatus& status =
        battery_update.power_status;

    const bool valid_battery_sample =
        status.voltage_mv > 0;
    const bool vbus_present =
        status.vbus_voltage_mv >= kChargingVbusPresentMv;

    if (!valid_battery_sample || !vbus_present) {
        state.full_confirm_count = 0;
        return false;
    }

    if (status.charge_state == nikos::board::ChargeState::Charging) {
        state.charging_observed = true;
        state.full_confirm_count = 0;
        return false;
    }

    const bool completion_candidate =
        state.charging_observed
        && status.voltage_mv >= kChargingFullBatteryMv
        && status.charge_state
            == nikos::board::ChargeState::Discharging;

    if (!completion_candidate) {
        state.full_confirm_count = 0;
        return false;
    }

    if (state.full_confirm_count < kChargingFullConfirmSamples) {
        ++state.full_confirm_count;
    }

    if (state.full_confirm_count < kChargingFullConfirmSamples) {
        return false;
    }

    state.full_latched = true;
    state.full_confirm_count = 0;
    return true;
}

void render_clock_glance(
    nikos::board::Board& board,
    nikos::clock::ClockService& clock_service)
{
    const nikos::clock::Reading reading = clock_service.read();
    char time_text[6]{};
    nikos::clock::ClockService::format_hhmm(
        reading,
        time_text,
        sizeof(time_text));

    board.clear_screen();
    board.draw_text_region(
        60,
        48,
        120,
        40,
        time_text,
        4,
        nikos::board::DisplayColor::PrimaryText,
        nikos::board::DisplayColor::Background);
}

void render_timer_alert(nikos::board::Board& board)
{
    board.clear_screen();
    board.draw_text_region(
        14,
        18,
        212,
        20,
        "MINUTNIK",
        2,
        nikos::board::DisplayColor::PrimaryText,
        nikos::board::DisplayColor::Background);
    board.draw_text_region(
        48,
        50,
        160,
        36,
        "KONIEC",
        4,
        nikos::board::DisplayColor::Attention,
        nikos::board::DisplayColor::Background);
    board.draw_text_region(
        35,
        112,
        190,
        14,
        "M5 / BOCZNY = ZAMKNIJ",
        1,
        nikos::board::DisplayColor::SecondaryText,
        nikos::board::DisplayColor::Background);
}

void render_battery_advisory(
    nikos::board::Board& board,
    nikos::battery_guard::AdvisoryLevel level)
{
    board.clear_screen();

    if (level == nikos::battery_guard::AdvisoryLevel::VeryLow) {
        board.draw_text_region(
            45,
            10,
            170,
            20,
            "BARDZO NISKA",
            2,
            nikos::board::DisplayColor::Danger,
            nikos::board::DisplayColor::Background);
        board.draw_text_region(
            76,
            32,
            100,
            20,
            "BATERIA",
            2,
            nikos::board::DisplayColor::Danger,
            nikos::board::DisplayColor::Background);
        board.draw_text_region(
            18,
            64,
            210,
            20,
            "PODLACZ LADOWARKE",
            2,
            nikos::board::DisplayColor::PrimaryText,
            nikos::board::DisplayColor::Background);
        board.draw_text_region(
            52,
            91,
            150,
            14,
            "LUB WYLACZ URZADZENIE",
            1,
            nikos::board::DisplayColor::SecondaryText,
            nikos::board::DisplayColor::Background);
    } else {
        board.draw_text_region(
            40,
            25,
            170,
            22,
            "NISKA BATERIA",
            2,
            nikos::board::DisplayColor::Attention,
            nikos::board::DisplayColor::Background);
        board.draw_text_region(
            18,
            61,
            210,
            22,
            "PODLACZ LADOWARKE",
            2,
            nikos::board::DisplayColor::PrimaryText,
            nikos::board::DisplayColor::Background);
    }

    board.draw_text_region(
        43,
        118,
        180,
        14,
        "M5 / BOCZNY = ZAMKNIJ",
        1,
        nikos::board::DisplayColor::SecondaryText,
        nikos::board::DisplayColor::Background);
}

void render_critical_battery_shutdown(nikos::board::Board& board)
{
    board.clear_screen();
    board.draw_text_region(
        40,
        28,
        170,
        22,
        "NISKA BATERIA",
        2,
        nikos::board::DisplayColor::Danger,
        nikos::board::DisplayColor::Background);
    board.draw_text_region(
        48,
        65,
        160,
        34,
        "WYLACZAM...",
        3,
        nikos::board::DisplayColor::PrimaryText,
        nikos::board::DisplayColor::Background);
}

[[noreturn]] void controlled_shutdown(
    nikos::board::Board& board,
    nikos::signal_sound::Player& signal_sound,
    nikos::communicator::CommunicatorApp& communicator,
    nikos::messaging::Service& messaging,
    nikos::radio::RadioService& radio)
{
    signal_sound.stop();
    board.stop_tone();
    communicator.reset_session();

    if (!messaging.stop()) {
        ESP_LOGW(
            kTag,
            "Messaging stop completed with cleanup errors during shutdown");
    }

    if (!radio.stop()) {
        ESP_LOGW(
            kTag,
            "Radio stop completed with cleanup errors during shutdown");
    }

    board.power_off();

    // M5Unified powerOff() should not return on target hardware.
    while (true) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

void redraw_runtime_ui(
    RuntimeState state,
    nikos::launcher::Launcher& launcher,
    nikos::communicator::CommunicatorApp& communicator,
    nikos::power_diag::PowerDiagApp& power_diag,
    nikos::power_diag::PowerDiagSession& power_diag_session,
    nikos::radiolab::RadioLabApp& radiolab)
{
    switch (state) {
        case RuntimeState::Communicator:
            communicator.redraw();
            break;
        case RuntimeState::PowerDiag:
            power_diag.redraw(power_diag_session.snapshot());
            break;
        case RuntimeState::RadioLab:
            radiolab.redraw();
            break;
        case RuntimeState::Launcher:
        default:
            launcher.redraw();
            break;
    }
}

bool initialize_nvs()
{
    esp_err_t result = nvs_flash_init();
    if (result == ESP_ERR_NVS_NO_FREE_PAGES
        || result == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        result = nvs_flash_erase();
        if (result == ESP_OK) {
            result = nvs_flash_init();
        }
    }

    if (result != ESP_OK) {
        ESP_LOGE(kTag, "NVS initialization failed: %s", esp_err_to_name(result));
        return false;
    }

    return true;
}

nikos::messaging::Config make_messaging_config(
    nikos::radio::Mode mode)
{
    nikos::messaging::Config config;
    config.channel = kRadioChannel;
    config.mode = mode;
    config.presence_interval_ms = 2000;
    config.presence_jitter_ms = 250;

    // Experimental logical-delivery policy. Keep these together so hardware
    // tests can tune sender behavior without changing delivery architecture.
    config.retry_interval_ms = 1000;
    config.retry_jitter_ms = 250;
    config.max_send_attempts = 8;
    config.delivery_timeout_ms = 12000;

    // Experimental TxResult pacing. MAC success only lengthens the wait for
    // the application ACK; it never completes logical delivery.
    config.tx_result_timeout_ms = 500;
    config.mac_success_ack_grace_margin_ms = 250;

    // Experimental receive/reachability profiles. These are configuration
    // values for validation, not permanent platform timing policy.
    config.foreground_rx = {1000, 500, 7000};
    config.background_rx = {3000, 500, 20000};
    return config;
}

nikos::launcher::CommunicatorStatus communicator_status(
    bool enabled,
    const nikos::messaging::Service& messaging)
{
    if (!enabled) {
        return nikos::launcher::CommunicatorStatus::Off;
    }
    if (!messaging.peer_known()) {
        return nikos::launcher::CommunicatorStatus::Searching;
    }
    return messaging.peer_reachable()
        ? nikos::launcher::CommunicatorStatus::Available
        : nikos::launcher::CommunicatorStatus::Ready;
}

nikos::launcher::CommunicatorDeliveryStatus communicator_delivery_status(
    nikos::communicator::CommunicatorApp::DeliveryFeedback feedback)
{
    using Feedback =
        nikos::communicator::CommunicatorApp::DeliveryFeedback;
    using Status = nikos::launcher::CommunicatorDeliveryStatus;

    switch (feedback) {
        case Feedback::Sending:
            return Status::Sending;
        case Feedback::Delivered:
            return Status::Delivered;
        case Feedback::Failed:
            return Status::Failed;
        case Feedback::None:
        default:
            return Status::None;
    }
}

nikos::power_diag::DisplayState power_diag_display_state(
    nikos::power::DisplayState state)
{
    using DiagState = nikos::power_diag::DisplayState;

    switch (state) {
        case nikos::power::DisplayState::Dimmed:
            return DiagState::Dimmed;
        case nikos::power::DisplayState::DisplayOff:
            return DiagState::Off;
        case nikos::power::DisplayState::Active:
        default:
            return DiagState::Active;
    }
}

nikos::power_diag::Observation make_power_diag_observation(
    RuntimeState runtime_state,
    const nikos::power::DisplayLifecycle& display_lifecycle,
    bool communicator_enabled,
    const nikos::messaging::Service& messaging)
{
    nikos::power_diag::Observation observation;
    observation.display_state =
        power_diag_display_state(display_lifecycle.state());
    observation.communicator_enabled = communicator_enabled;
    observation.communicator_foreground =
        runtime_state == RuntimeState::Communicator;
    observation.radiolab_foreground =
        runtime_state == RuntimeState::RadioLab;
    observation.radio_mode =
        messaging.radio_mode() == nikos::radio::Mode::Lr
            ? nikos::power_diag::RadioMode::Lr
            : nikos::power_diag::RadioMode::Normal;

    if (!communicator_enabled) {
        return observation;
    }

    observation.rx_profile =
        messaging.rx_profile() == nikos::messaging::RxProfile::Foreground
            ? nikos::power_diag::RxProfile::Foreground
            : nikos::power_diag::RxProfile::Background;

    const nikos::messaging::RxSchedule schedule =
        messaging.current_rx_schedule();
    observation.rx_interval_ms = schedule.interval_ms;
    observation.rx_wake_window_ms = schedule.wake_window_ms;
    observation.peer_known = messaging.peer_known();
    observation.peer_reachable = messaging.peer_reachable();

    std::int8_t rssi = 0;
    if (messaging.latest_peer_rssi(rssi)) {
        observation.rssi_valid = true;
        observation.rssi = rssi;
    }

    return observation;
}

}  // namespace

extern "C" void app_main(void)
{
    nikos::board::Board board;
    board.begin();

    if (!board.initialize_rtc()) {
        ESP_LOGW(kTag, "RTC unavailable; clock will display --:--");
    }
    nikos::clock::ClockService clock_service(board);

    nikos::power::DisplayLifecycle display_lifecycle(board);
    display_lifecycle.begin();

    nikos::settings::State settings;
    board.set_theme(settings.theme);

    nikos::signal_sound::Player signal_sound(board, settings);
    nikos::countdown::Service countdown(esp_timer_get_time);
    nikos::battery_guard::BatteryGuard battery_guard(board);
    ChargingModeState charging_mode;
    (void)poll_charging_cable(board, charging_mode, now_ms());

    nikos::launcher::Launcher launcher(
        board,
        clock_service,
        countdown,
        settings,
        signal_sound);

    if (charging_mode.active) {
        // Keep ordinary boot UI hidden when VBUS is already present. Runtime
        // state still initializes normally below and the charging screen is
        // shown once composition is complete.
        display_lifecycle.display_off_now();
    } else {
        launcher.show_splash();
        vTaskDelay(pdMS_TO_TICKS(kSplashDurationMs));
    }

    if (!initialize_nvs()) {
        board.draw_screen(
            "NIKOS OS",
            "NVS INIT FAILED\n"
            "Check serial log for the failing step.");
        while (true) {
            board.poll_input();
            vTaskDelay(pdMS_TO_TICKS(100));
        }
    }

    nikos::radio::RadioService radio;
    nikos::messaging::Service messaging(radio);
    nikos::communicator::CommunicatorApp communicator(
        board,
        messaging,
        display_lifecycle,
        signal_sound,
        settings,
        "DRUGI M5");
    nikos::radiolab::RadioLabApp radiolab(board, radio);
    nikos::power_diag::PowerDiagSession power_diag_session;
    nikos::power_diag::PowerDiagApp power_diag(board);

    bool communicator_enabled = false;
    bool resume_messaging_after_radiolab = false;

    RuntimeState state = RuntimeState::Launcher;
    bool clock_glance_active = false;
    std::uint32_t clock_glance_started_ms = 0;
    bool timer_alert_visible = false;
    bool timer_deferred_for_communication = false;
    bool battery_advisory_visible = false;
    nikos::battery_guard::AdvisoryLevel battery_advisory_level =
        nikos::battery_guard::AdvisoryLevel::None;

    if (charging_mode.active) {
        display_lifecycle.display_off_now();
        launcher.begin(communicator_status(communicator_enabled, messaging));
        show_charging_presentation(
            board,
            display_lifecycle,
            charging_mode,
            now_ms());
    } else {
        display_lifecycle.note_visible_activity();
        launcher.begin(communicator_status(communicator_enabled, messaging));
    }

    while (true) {
        // update() advances delivery only while messaging owns active
        // transport. A RadioLab handoff intentionally freezes retry/deadline
        // timing until messaging transport resumes.
        messaging.update();
        const bool delivery_feedback_changed =
            communicator.service_delivery();
        launcher.set_communicator_delivery_status(
            communicator_delivery_status(
                communicator.delivery_feedback()),
            false);

        countdown.update();
        signal_sound.update();

        const std::uint32_t loop_now_ms = now_ms();
        const ChargingCableEvent charging_cable_event =
            poll_charging_cable(
                board,
                charging_mode,
                loop_now_ms);

        if (charging_cable_event == ChargingCableEvent::Connected) {
            clock_glance_active = false;
            timer_alert_visible = false;
            battery_advisory_visible = false;
            battery_advisory_level =
                nikos::battery_guard::AdvisoryLevel::None;

            show_charging_presentation(
                board,
                display_lifecycle,
                charging_mode,
                loop_now_ms);
        } else if (
            charging_cable_event == ChargingCableEvent::Disconnected) {
            clock_glance_active = false;
            timer_alert_visible = false;
            battery_advisory_visible = false;
            battery_advisory_level =
                nikos::battery_guard::AdvisoryLevel::None;

            // Return immediately to the existing runtime without rebooting or
            // resetting application state. Keep any M5/BOCZNY gesture that
            // began under Charging Mode from completing in the restored UI.
            display_lifecycle.suppress_user_gesture_until_release();
            display_lifecycle.note_visible_activity();
            redraw_runtime_ui(
                state,
                launcher,
                communicator,
                power_diag,
                power_diag_session,
                radiolab);
        }

        const nikos::battery_guard::UpdateResult battery_update =
            battery_guard.update(loop_now_ms);
        if (battery_update.sampled) {
            power_diag_session.record_battery_sample(
                battery_update.power_status);
        }

        if (battery_update.critical_confirmed) {
            clock_glance_active = false;
            battery_advisory_visible = false;

            signal_sound.stop();
            board.stop_tone();
            display_lifecycle.note_visible_activity();
            render_critical_battery_shutdown(board);
            vTaskDelay(pdMS_TO_TICKS(kCriticalShutdownMessageMs));

            controlled_shutdown(
                board,
                signal_sound,
                communicator,
                messaging,
                radio);
        }

        if (update_charging_full_confirmation(
                charging_mode,
                battery_update)) {
            board.tone(kChargingFullToneHz, kChargingFullToneMs);
            show_charging_presentation(
                board,
                display_lifecycle,
                charging_mode,
                loop_now_ms);
        }

        if (battery_update.charging_detected
            && battery_advisory_visible) {
            battery_advisory_visible = false;
            battery_advisory_level =
                nikos::battery_guard::AdvisoryLevel::None;

            if (!charging_mode.active) {
                redraw_runtime_ui(
                    state,
                    launcher,
                    communicator,
                    power_diag,
                    power_diag_session,
                    radiolab);
            }
        }

        if (charging_mode.active) {
            const nikos::board::InputState charging_input =
                board.poll_input();

            if (charging_wake_requested(charging_input)) {
                show_charging_presentation(
                    board,
                    display_lifecycle,
                    charging_mode,
                    loop_now_ms);
            }

            update_charging_presentation(
                board,
                display_lifecycle,
                charging_mode,
                loop_now_ms);

            if (state == RuntimeState::RadioLab) {
                (void)radiolab.update(
                    nikos::board::InputState{},
                    false);
            }

            if (power_diag_session.running()) {
                power_diag_session.observe(
                    monotonic_now_us(),
                    make_power_diag_observation(
                        state,
                        display_lifecycle,
                        communicator_enabled,
                        messaging));
            }

            vTaskDelay(pdMS_TO_TICKS(kLoopDelayMs));
            continue;
        }

        const nikos::power::FilteredInput display_input =
            display_lifecycle.filter_input(board.poll_input());
        const nikos::board::InputState& input = display_input.input;

        if (display_input.power_display_off) {
            // POWER controls display visibility only. Transient visible
            // presentations end, but a battery advisory remains pending until
            // an M5/BOCZNY dismissal or charging/recovery policy clears it.
            clock_glance_active = false;
            battery_advisory_visible = false;
        }

        if (timer_deferred_for_communication
            && (state != RuntimeState::Communicator
                || !communicator.timer_preemption_active())) {
            timer_deferred_for_communication = false;
        }

        const bool timer_expired = countdown.expired_pending();
        const bool user_woke_display =
            display_input.wake_reason
            == nikos::power::WakeReason::UserButton;
        const bool user_started_clock_glance =
            user_woke_display && !timer_expired;
        if (user_started_clock_glance) {
            clock_glance_active = true;
            clock_glance_started_ms = now_ms();
        }

        display_lifecycle.update();

        if (power_diag_session.running()) {
            power_diag_session.observe(
                monotonic_now_us(),
                make_power_diag_observation(
                    state,
                    display_lifecycle,
                    communicator_enabled,
                    messaging));
        }

        bool timer_alert_presented_now = false;

        if (timer_expired
            && !timer_deferred_for_communication
            && state == RuntimeState::Communicator
            && communicator.timer_preemption_active()) {
            timer_alert_visible = false;
            timer_deferred_for_communication = true;
            clock_glance_active = false;
        }

        if (timer_expired
            && !timer_deferred_for_communication
            && state != RuntimeState::RadioLab
            && communicator_enabled
            && communicator.process_incoming()) {
            timer_alert_visible = false;
            timer_deferred_for_communication = true;
            clock_glance_active = false;
            battery_advisory_visible = false;

            if (any_user_button_activity(input)) {
                display_lifecycle.suppress_user_gesture_until_release();
            }

            if (state == RuntimeState::Launcher
                || state == RuntimeState::PowerDiag) {
                communicator.begin();
                state = RuntimeState::Communicator;
            }

            vTaskDelay(pdMS_TO_TICKS(kLoopDelayMs));
            continue;
        }

        if (timer_expired && !timer_deferred_for_communication) {
            clock_glance_active = false;

            if (!timer_alert_visible) {
                battery_advisory_visible = false;
                display_lifecycle.note_visible_activity();
                signal_sound.play_selected();
                render_timer_alert(board);
                timer_alert_visible = true;
                timer_alert_presented_now = true;

                // A button gesture already in flight when the alert appears
                // belongs to the obscured UI, not to Timer dismissal.
                if (any_user_button_activity(input)) {
                    display_lifecycle.suppress_user_gesture_until_release();
                }
            } else if (user_woke_display) {
                // DisplayOff wake remains a consumed first gesture. Repaint
                // the still-pending alert without restarting its finite audio.
                render_timer_alert(board);
            }
        }

        if (timer_alert_visible) {
            if (state == RuntimeState::RadioLab) {
                (void)radiolab.update(
                    nikos::board::InputState{},
                    false);
            }

            if (!timer_alert_presented_now
                && any_user_button_activity(input)) {
                signal_sound.stop();
                countdown.acknowledge_expiration();
                timer_alert_visible = false;
                timer_deferred_for_communication = false;
                clock_glance_active = false;

                display_lifecycle.suppress_user_gesture_until_release();
                display_lifecycle.note_visible_activity();
                redraw_runtime_ui(
                state,
                launcher,
                communicator,
                power_diag,
                power_diag_session,
                radiolab);
            }

            vTaskDelay(pdMS_TO_TICKS(kLoopDelayMs));
            continue;
        }

        const nikos::battery_guard::AdvisoryLevel
            pending_battery_advisory =
                battery_guard.pending_advisory();
        const bool battery_advisory_pending =
            pending_battery_advisory
            != nikos::battery_guard::AdvisoryLevel::None;

        if ((battery_advisory_visible || battery_advisory_pending)
            && state != RuntimeState::RadioLab
            && communicator_enabled
            && (state != RuntimeState::Communicator
                || !communicator.timer_preemption_active())
            && communicator.process_incoming()) {
            battery_advisory_visible = false;
            clock_glance_active = false;

            if (any_user_button_activity(input)) {
                display_lifecycle.suppress_user_gesture_until_release();
            }

            if (state == RuntimeState::Launcher
                || state == RuntimeState::PowerDiag) {
                communicator.begin();
                state = RuntimeState::Communicator;
            }

            vTaskDelay(pdMS_TO_TICKS(kLoopDelayMs));
            continue;
        }

        if (battery_advisory_visible) {
            if (display_lifecycle.state()
                == nikos::power::DisplayState::DisplayOff) {
                battery_advisory_visible = false;
            } else {
                if (pending_battery_advisory
                        == nikos::battery_guard::AdvisoryLevel::VeryLow
                    && battery_advisory_level
                        != nikos::battery_guard::AdvisoryLevel::VeryLow) {
                    battery_advisory_level =
                        nikos::battery_guard::AdvisoryLevel::VeryLow;
                    render_battery_advisory(
                        board,
                        battery_advisory_level);
                }

                if (any_user_button_activity(input)) {
                    battery_guard.acknowledge_advisory();
                    battery_advisory_visible = false;
                    display_lifecycle.suppress_user_gesture_until_release();
                    display_lifecycle.note_visible_activity();
                    redraw_runtime_ui(
                state,
                launcher,
                communicator,
                power_diag,
                power_diag_session,
                radiolab);

                    vTaskDelay(pdMS_TO_TICKS(kLoopDelayMs));
                    continue;
                } else {
                    if (state == RuntimeState::RadioLab) {
                        (void)radiolab.update(
                            nikos::board::InputState{},
                            false);
                    }

                    vTaskDelay(pdMS_TO_TICKS(kLoopDelayMs));
                    continue;
                }
            }
        }

        if (!battery_advisory_visible
            && battery_advisory_pending
            && display_lifecycle.state()
                != nikos::power::DisplayState::DisplayOff
            && !(state == RuntimeState::Communicator
                && communicator.timer_preemption_active())) {
            clock_glance_active = false;
            battery_advisory_visible = true;
            battery_advisory_level = pending_battery_advisory;
            render_battery_advisory(board, battery_advisory_level);

            // A gesture already in flight belongs to the obscured UI, not to
            // the advisory that has just appeared.
            if (any_user_button_activity(input)) {
                display_lifecycle.suppress_user_gesture_until_release();
            }

            if (state == RuntimeState::RadioLab) {
                (void)radiolab.update(
                    nikos::board::InputState{},
                    false);
            }

            vTaskDelay(pdMS_TO_TICKS(kLoopDelayMs));
            continue;
        }

        if (clock_glance_active) {
            bool communication_accepted = false;

            if (state != RuntimeState::RadioLab
                && communicator_enabled
                && communicator.process_incoming()) {
                communication_accepted = true;
                clock_glance_active = false;

                // If the user also began the second glance gesture on this
                // iteration, keep it from leaking into the incoming UI.
                if (any_user_button_activity(input)) {
                    display_lifecycle.suppress_user_gesture_until_release();
                }

                if (state == RuntimeState::Launcher
                    || state == RuntimeState::PowerDiag) {
                    communicator.begin();
                    state = RuntimeState::Communicator;
                }
            }

            if (communication_accepted) {
                vTaskDelay(pdMS_TO_TICKS(kLoopDelayMs));
                continue;
            }

            if (state == RuntimeState::RadioLab) {
                (void)radiolab.update(
                    nikos::board::InputState{},
                    false);
            }

            if (user_started_clock_glance) {
                render_clock_glance(board, clock_service);
            } else if (any_user_button_activity(input)) {
                display_lifecycle.suppress_user_gesture_until_release();
                display_lifecycle.note_visible_activity();
                clock_glance_active = false;
                redraw_runtime_ui(
                state,
                launcher,
                communicator,
                power_diag,
                power_diag_session,
                radiolab);
            } else if (
                now_ms() - clock_glance_started_ms
                    >= kClockGlanceDurationMs) {
                clock_glance_active = false;
                display_lifecycle.display_off_now();
            }

            vTaskDelay(pdMS_TO_TICKS(kLoopDelayMs));
            continue;
        }

        if (state == RuntimeState::Launcher) {
            const bool launcher_visible =
                display_lifecycle.state()
                != nikos::power::DisplayState::DisplayOff;
            launcher.set_communicator_status(
                communicator_status(communicator_enabled, messaging),
                launcher_visible);
            if (communicator_enabled
                && communicator.process_incoming()) {
                communicator.begin();
                state = RuntimeState::Communicator;
            } else if (launcher_visible) {
                if (delivery_feedback_changed) {
                    launcher.redraw();
                }

                const nikos::launcher::Action action = launcher.update(input);

                if (action == nikos::launcher::Action::StartCommunicator) {
                    communicator.reset_session();

                    if (messaging.begin(
                            make_messaging_config(
                                messaging.radio_mode()))) {
                        communicator_enabled = true;
                        display_lifecycle.note_visible_activity();

                        communicator.begin();
                        state = RuntimeState::Communicator;
                    } else {
                        ESP_LOGW(
                            kTag,
                            "Communicator messaging failed to start");
                        display_lifecycle.note_visible_activity();
                        launcher.begin(nikos::launcher::CommunicatorStatus::Off);
                    }
                } else if (
                    action == nikos::launcher::Action::OpenCommunicator) {
                    display_lifecycle.note_visible_activity();
                    communicator.begin();
                    state = RuntimeState::Communicator;
                } else if (
                    action == nikos::launcher::Action::StopCommunicator) {
                    communicator.reset_session();
                    launcher.set_communicator_delivery_status(
                        nikos::launcher::CommunicatorDeliveryStatus::None,
                        false);

                    if (!messaging.stop()) {
                        ESP_LOGW(
                            kTag,
                            "Communicator messaging stop completed with cleanup errors");
                    }

                    communicator_enabled = false;
                    display_lifecycle.note_visible_activity();
                    launcher.begin(nikos::launcher::CommunicatorStatus::Off);
                } else if (
                    action == nikos::launcher::Action::ShutdownRequested) {
                    controlled_shutdown(
                        board,
                        signal_sound,
                        communicator,
                        messaging,
                        radio);
                } else if (
                    action == nikos::launcher::Action::OpenPowerDiag) {
                    display_lifecycle.note_visible_activity();
                    power_diag.begin(power_diag_session.snapshot());
                    state = RuntimeState::PowerDiag;
                } else if (
                    action == nikos::launcher::Action::OpenRadioLab) {
                    resume_messaging_after_radiolab =
                        communicator_enabled;

                    if (resume_messaging_after_radiolab
                        && !messaging.pause_transport()) {
                        ESP_LOGW(
                            kTag,
                            "Messaging transport pause completed with cleanup errors");
                    }

                    if (radio.begin(
                            kRadioChannel,
                            nikos::radio::Mode::Normal)) {
                        display_lifecycle.note_visible_activity();
                        radiolab.begin();
                        state = RuntimeState::RadioLab;
                    } else {
                        display_lifecycle.note_visible_activity();
                        board.draw_screen(
                            "RADIO LAB",
                            "RADIO INIT FAILED\n"
                            "Check serial log.");
                        vTaskDelay(pdMS_TO_TICKS(kRadioErrorDisplayMs));

                        if (resume_messaging_after_radiolab
                            && !messaging.resume_transport()) {
                            ESP_LOGW(
                                kTag,
                                "Messaging transport failed to resume after RadioLab error; disabling Communicator session");
                            communicator.reset_session();
                            launcher.set_communicator_delivery_status(
                                nikos::launcher::CommunicatorDeliveryStatus::None,
                                false);
                            (void)messaging.stop();
                            communicator_enabled = false;
                        }

                        resume_messaging_after_radiolab = false;
                        display_lifecycle.note_visible_activity();
                        launcher.begin_tools(
                            communicator_status(communicator_enabled, messaging));
                    }
                }
            }
        } else if (state == RuntimeState::Communicator) {
            if (communicator.update(input)
                == nikos::communicator::CommunicatorApp::UpdateResult::ExitRequested) {
                communicator.end();

                display_lifecycle.note_visible_activity();
                launcher.begin(
                    communicator_status(communicator_enabled, messaging));
                state = RuntimeState::Launcher;
            } else if (
                delivery_feedback_changed
                && display_lifecycle.state()
                    != nikos::power::DisplayState::DisplayOff
                && communicator.delivery_feedback_overlay_allowed()) {
                communicator.redraw();
            }
        } else if (state == RuntimeState::PowerDiag) {
            if (communicator_enabled
                && communicator.process_incoming()) {
                communicator.begin();
                state = RuntimeState::Communicator;
            } else if (
                display_lifecycle.state()
                != nikos::power::DisplayState::DisplayOff) {
                const nikos::power_diag::PowerDiagApp::UpdateResult result =
                    power_diag.update(
                        input,
                        power_diag_session.snapshot());

                if (result
                    == nikos::power_diag::PowerDiagApp::UpdateResult::StartRequested
                    || result
                    == nikos::power_diag::PowerDiagApp::UpdateResult::NewTestRequested) {
                    power_diag_session.start(
                        monotonic_now_us(),
                        make_power_diag_observation(
                            state,
                            display_lifecycle,
                            communicator_enabled,
                            messaging));
                    power_diag.redraw(power_diag_session.snapshot());
                } else if (
                    result
                    == nikos::power_diag::PowerDiagApp::UpdateResult::ExitRequested) {
                    display_lifecycle.note_visible_activity();
                    launcher.begin_tools(
                        communicator_status(communicator_enabled, messaging));
                    state = RuntimeState::Launcher;
                }
            }
        } else {
            const bool radiolab_visible =
                display_lifecycle.state()
                != nikos::power::DisplayState::DisplayOff;
            if (radiolab.update(input, radiolab_visible)
                == nikos::radiolab::RadioLabApp::UpdateResult::ExitRequested) {
                radiolab.end();

                if (!radio.stop()) {
                    ESP_LOGW(
                        kTag,
                        "Radio stop completed with cleanup errors");
                }

                if (resume_messaging_after_radiolab
                    && !messaging.resume_transport()) {
                    ESP_LOGW(
                        kTag,
                        "Messaging transport failed to resume after RadioLab; disabling Communicator session");
                    communicator.reset_session();
                    launcher.set_communicator_delivery_status(
                        nikos::launcher::CommunicatorDeliveryStatus::None,
                        false);
                    (void)messaging.stop();
                    communicator_enabled = false;
                }

                resume_messaging_after_radiolab = false;
                display_lifecycle.note_visible_activity();
                launcher.begin_tools(
                    communicator_status(communicator_enabled, messaging));
                state = RuntimeState::Launcher;
            }
        }

        vTaskDelay(pdMS_TO_TICKS(kLoopDelayMs));
    }

}
