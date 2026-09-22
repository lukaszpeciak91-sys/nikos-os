#include "board/board.hpp"
#include "clock/clock_service.hpp"
#include "communicator/communicator_app.hpp"
#include "countdown/countdown_service.hpp"
#include "launcher/launcher.hpp"
#include "messaging/messaging_service.hpp"
#include "power/display_lifecycle.hpp"
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

enum class RuntimeState : std::uint8_t {
    Launcher,
    Communicator,
    RadioLab,
};

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

void redraw_runtime_ui(
    RuntimeState state,
    nikos::launcher::Launcher& launcher,
    nikos::communicator::CommunicatorApp& communicator,
    nikos::radiolab::RadioLabApp& radiolab)
{
    switch (state) {
        case RuntimeState::Communicator:
            communicator.redraw();
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
    nikos::launcher::Launcher launcher(
        board,
        clock_service,
        countdown,
        settings,
        signal_sound);
    launcher.show_splash();
    vTaskDelay(pdMS_TO_TICKS(kSplashDurationMs));

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
        "DRUGI M5");
    nikos::radiolab::RadioLabApp radiolab(board, radio);

    bool communicator_enabled = false;
    bool resume_messaging_after_radiolab = false;

    RuntimeState state = RuntimeState::Launcher;
    bool clock_glance_active = false;
    std::uint32_t clock_glance_started_ms = 0;
    bool timer_alert_visible = false;
    bool timer_deferred_for_communication = false;

    display_lifecycle.note_visible_activity();
    launcher.begin(communicator_status(communicator_enabled, messaging));

    while (true) {
        // update() advances delivery only while messaging owns active
        // transport. A RadioLab handoff intentionally freezes retry/deadline
        // timing until messaging transport resumes.
        messaging.update();
        countdown.update();
        signal_sound.update();

        const nikos::power::FilteredInput display_input =
            display_lifecycle.filter_input(board.poll_input());
        const nikos::board::InputState& input = display_input.input;

        if (timer_deferred_for_communication
            && state == RuntimeState::Launcher) {
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

        bool timer_alert_presented_now = false;

        if (timer_expired
            && !timer_deferred_for_communication
            && state == RuntimeState::Communicator
            && communicator.attention_alert_active()) {
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

            if (any_user_button_activity(input)) {
                display_lifecycle.suppress_user_gesture_until_release();
            }

            if (state == RuntimeState::Launcher) {
                if (!communicator.begin()) {
                    ESP_LOGW(
                        kTag,
                        "Communicator foreground RX profile could not be applied");
                }
                state = RuntimeState::Communicator;
            }

            vTaskDelay(pdMS_TO_TICKS(kLoopDelayMs));
            continue;
        }

        if (timer_expired && !timer_deferred_for_communication) {
            clock_glance_active = false;

            if (!timer_alert_visible) {
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
                    radiolab);
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

                if (state == RuntimeState::Launcher) {
                    if (!communicator.begin()) {
                        ESP_LOGW(
                            kTag,
                            "Communicator foreground RX profile could not be applied");
                    }
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
            launcher.set_communicator_status(
                communicator_status(communicator_enabled, messaging));
            if (communicator_enabled
                && communicator.process_incoming()) {
                if (!communicator.begin()) {
                    ESP_LOGW(
                        kTag,
                        "Communicator foreground RX profile could not be applied");
                }
                state = RuntimeState::Communicator;
            } else {
                const nikos::launcher::Action action = launcher.update(input);

                if (action == nikos::launcher::Action::StartCommunicator) {
                    communicator.reset_session();

                    if (messaging.begin(
                            make_messaging_config(
                                messaging.radio_mode()))) {
                        communicator_enabled = true;
                        display_lifecycle.note_visible_activity();

                        if (!communicator.begin()) {
                            ESP_LOGW(
                                kTag,
                                "Communicator foreground RX profile could not be applied");
                        }
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
                    if (!communicator.begin()) {
                        ESP_LOGW(
                            kTag,
                            "Communicator foreground RX profile could not be applied");
                    }
                    state = RuntimeState::Communicator;
                } else if (
                    action == nikos::launcher::Action::StopCommunicator) {
                    communicator.reset_session();

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
                    board.stop_tone();
                    communicator.reset_session();

                    if (!messaging.stop()) {
                        ESP_LOGW(
                            kTag,
                            "Messaging stop completed with cleanup errors during shutdown");
                    }

                    communicator_enabled = false;
                    resume_messaging_after_radiolab = false;

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
                if (!communicator.end()) {
                    ESP_LOGW(
                        kTag,
                        "Communicator background RX profile could not be restored");
                }

                display_lifecycle.note_visible_activity();
                launcher.begin(
                    communicator_status(communicator_enabled, messaging));
                state = RuntimeState::Launcher;
            }
        } else {
            if (radiolab.update(input)
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
