#include "communicator/communicator_app.hpp"

#include <cstddef>

#include "esp_log.h"
#include "esp_timer.h"

namespace {

constexpr char kTag[] = "communicator";
constexpr std::uint32_t kNotificationToneMs = 90;
constexpr float kNotificationToneHz = 2600.0F;

constexpr std::uint32_t kSignalAnimationMs = 120;
constexpr std::uint8_t kSignalPlaybackCount = 10;
constexpr std::uint32_t kDeliveryResultVisibleMs = 2500;

constexpr std::int16_t kScreenWidth = 240;
constexpr std::int16_t kScreenHeight = 135;

std::uint8_t preset_text_scale(
    nikos::communicator::catalogue::PresetId preset)
{
    using nikos::communicator::catalogue::PresetId;
    return preset == PresetId::Walk ? 2 : 3;
}

std::uint8_t response_text_scale(
    nikos::communicator::catalogue::ResponseId)
{
    return 3;
}

std::uint8_t context_text_scale(
    nikos::communicator::catalogue::PresetId preset)
{
    using nikos::communicator::catalogue::PresetId;
    return preset == PresetId::Walk ? 1 : 2;
}

void draw_selection_marker(
    nikos::board::Board& board,
    std::int16_t x,
    std::int16_t y,
    std::int16_t height,
    nikos::board::DisplayColor color =
        nikos::board::DisplayColor::Accent)
{
    const std::int16_t bottom =
        static_cast<std::int16_t>(y + height - 1);
    board.draw_line(x, y, x, bottom, color);
    board.draw_line(
        static_cast<std::int16_t>(x + 1),
        y,
        static_cast<std::int16_t>(x + 1),
        bottom,
        color);
}

const char* choice_counter(std::uint8_t index, std::uint8_t count)
{
    if (count <= 1U) {
        return "1 / 1";
    }
    if (count == 2U) {
        return index == 0U ? "1 / 2" : "2 / 2";
    }

    switch (index) {
        case 1U:
            return "2 / 3";
        case 2U:
            return "3 / 3";
        case 0U:
        default:
            return "1 / 3";
    }
}

std::uint32_t now_ms()
{
    return static_cast<std::uint32_t>(esp_timer_get_time() / 1000U);
}

}  // namespace

namespace nikos::communicator {

CommunicatorApp::CommunicatorApp(
    board::Board& board,
    messaging::Service& messaging,
    power::DisplayLifecycle& display_lifecycle,
    signal_sound::Player& signal_sound,
    const char* peer_label)
    : board_(board),
      messaging_(messaging),
      display_lifecycle_(display_lifecycle),
      signal_sound_(signal_sound),
      peer_label_(peer_label)
{
}

void CommunicatorApp::begin()
{
    active_ = true;
    foreground_exit_requested_ = false;
    if (!signal_alert_active_) {
        render_current();
    }
}

void CommunicatorApp::end()
{
    if (signal_alert_active_) {
        signal_sound_.stop();
        signal_alert_active_ = false;
        signal_return_to_launcher_ = false;
        signal_audio_complete_rendered_ = false;
    }
    signal_playback_cycles_started_ = 0;

    active_ = false;
}

void CommunicatorApp::redraw()
{
    if (!active_) {
        return;
    }

    if (signal_alert_active_) {
        render_signal_alert(signal_animation_wide_);
    } else if (signal_unavailable_feedback_) {
        render_signal_unavailable();
    } else {
        render_current();
    }
}


void CommunicatorApp::reset_session()
{
    signal_sound_.stop();

    active_ = false;
    foreground_exit_requested_ = false;
    options_active_ = false;
    radio_mode_change_failed_ = false;
    state_ = State::Main;

    selected_main_index_ = 0;
    selected_options_index_ = 0;
    selected_response_index_ = 0;

    incoming_preset_ = catalogue::PresetId::Greeting;
    incoming_response_ = catalogue::ResponseId::GreetingHello;
    response_set_ = catalogue::ResponseSet{};

    latest_outgoing_message_id_ = 0;
    latest_delivery_feedback_ = DeliveryFeedback::None;
    delivery_result_started_ms_ = 0;

    signal_unavailable_feedback_ = false;
    signal_alert_active_ = false;
    signal_return_to_launcher_ = false;
    signal_audio_complete_rendered_ = false;
    signal_animation_wide_ = false;
    signal_playback_cycles_started_ = 0;
    signal_last_animation_ms_ = 0;

    rendered_peer_state_valid_ = false;
    rendered_peer_known_ = false;
    rendered_peer_reachable_ = false;
    rendered_signal_bars_ = 0;
}

bool CommunicatorApp::service_delivery()
{
    bool changed = false;

    messaging::DeliveryReceipt receipt;
    if (messaging_.poll_delivery(receipt)) {
        const DeliveryFeedback before = latest_delivery_feedback_;
        handle_delivery_receipt(receipt);
        changed = latest_delivery_feedback_ != before;
    }

    if ((latest_delivery_feedback_ == DeliveryFeedback::Delivered
            || latest_delivery_feedback_ == DeliveryFeedback::Failed)
        && now_ms() - delivery_result_started_ms_
            >= kDeliveryResultVisibleMs) {
        latest_delivery_feedback_ = DeliveryFeedback::None;
        latest_outgoing_message_id_ = 0;
        delivery_result_started_ms_ = 0;
        changed = true;
    }

    return changed;
}

CommunicatorApp::DeliveryFeedback CommunicatorApp::delivery_feedback() const
{
    return latest_delivery_feedback_;
}

bool CommunicatorApp::delivery_feedback_overlay_allowed() const
{
    return active_
        && state_ == State::Main
        && !signal_alert_active_
        && !signal_unavailable_feedback_;
}

CommunicatorApp::UpdateResult CommunicatorApp::update(
    const board::InputState& input)
{
    if (!signal_alert_active_) {
        (void)process_incoming();
    }

    if (signal_alert_active_) {
        update_signal_alert(now_ms());

        if (any_user_button(input)) {
            const bool return_to_launcher = signal_return_to_launcher_;
            dismiss_signal_alert();
            return return_to_launcher
                ? UpdateResult::ExitRequested
                : UpdateResult::Running;
        }

        return UpdateResult::Running;
    }

    if (state_ == State::Main
        && !options_active_
        && display_lifecycle_.state()
            != power::DisplayState::DisplayOff) {
        render_main_if_status_changed();
    }

    if (input.secondary_long) {
        if (state_ == State::Main && options_active_) {
            options_active_ = false;
            radio_mode_change_failed_ = false;
            render_main();
            return UpdateResult::Running;
        }

        return UpdateResult::ExitRequested;
    }

    handle_input(input);

    if (foreground_exit_requested_) {
        foreground_exit_requested_ = false;
        return UpdateResult::ExitRequested;
    }

    return UpdateResult::Running;
}

bool CommunicatorApp::timer_preemption_active() const
{
    if (signal_alert_active_) {
        return true;
    }

    switch (state_) {
        case State::IncomingPreset:
        case State::ChoosingResponse:
        case State::IncomingResponse:
        case State::WaitDecision:
            return true;
        case State::Main:
        default:
            return false;
    }
}

bool CommunicatorApp::valid_user_message(
    const messaging::IncomingMessage& message) const
{
    catalogue::PresetId preset;
    if (!catalogue::preset_from_wire(message.preset_id, preset)) {
        return false;
    }

    if (message.kind == messaging::IncomingKind::PresetMessage) {
        return true;
    }

    if (message.kind != messaging::IncomingKind::PresetResponse) {
        return false;
    }

    catalogue::ResponseId response;
    return catalogue::response_from_wire(message.response_id, response)
        && catalogue::response_allowed_for(preset, response);
}

bool CommunicatorApp::accept_incoming(
    const messaging::IncomingMessage& message)
{
    if (!valid_user_message(message)) {
        return false;
    }

    catalogue::PresetId preset;
    (void)catalogue::preset_from_wire(message.preset_id, preset);

    incoming_preset_ = preset;
    options_active_ = false;
    radio_mode_change_failed_ = false;

    if (message.kind == messaging::IncomingKind::PresetMessage) {
        response_set_ = catalogue::responses_for(preset);
        selected_response_index_ = 0;
        state_ = State::IncomingPreset;
        notify_incoming();
        return true;
    }

    catalogue::ResponseId response;
    (void)catalogue::response_from_wire(message.response_id, response);
    incoming_response_ = response;
    state_ = catalogue::needs_wait_decision(response)
        ? State::WaitDecision
        : State::IncomingResponse;
    notify_incoming();
    return true;
}

bool CommunicatorApp::process_incoming()
{
    messaging::IncomingMessage latest_user_message{};
    bool latest_user_message_valid = false;
    bool ring_received = false;

    messaging::IncomingMessage incoming;
    while (messaging_.peek_incoming(incoming)) {
        if (!messaging_.consume_incoming(incoming.logical_message_id)) {
            break;
        }

        if (incoming.kind == messaging::IncomingKind::Ring) {
            ring_received = true;
            continue;
        }

        if (valid_user_message(incoming)) {
            latest_user_message = incoming;
            latest_user_message_valid = true;
        } else {
            ESP_LOGW(
                kTag,
                "Discard invalid user message id=%lu preset=%u response=%u",
                static_cast<unsigned long>(incoming.logical_message_id),
                static_cast<unsigned>(incoming.preset_id),
                static_cast<unsigned>(incoming.response_id));
        }
    }

    bool accepted = false;
    bool user_message_accepted = false;
    if (latest_user_message_valid) {
        user_message_accepted = accept_incoming(latest_user_message);
        accepted = user_message_accepted;
    }

    if (ring_received && !signal_alert_active_) {
        start_signal_alert();

        // A background RING by itself still returns to Launcher after
        // dismissal. If this same drain also accepted a user-visible message,
        // the transient signal must reveal that retained message instead.
        if (user_message_accepted) {
            signal_return_to_launcher_ = false;
        }

        accepted = true;
    }

    return accepted;
}

void CommunicatorApp::handle_delivery_receipt(
    const messaging::DeliveryReceipt& receipt)
{
    if (receipt.logical_message_id != latest_outgoing_message_id_) {
        ESP_LOGI(
            kTag,
            "Ignore stale delivery receipt id=%lu latest=%lu",
            static_cast<unsigned long>(receipt.logical_message_id),
            static_cast<unsigned long>(latest_outgoing_message_id_));
        return;
    }

    latest_delivery_feedback_ =
        receipt.outcome == messaging::DeliveryOutcome::Delivered
        ? DeliveryFeedback::Delivered
        : DeliveryFeedback::Failed;
    delivery_result_started_ms_ = now_ms();

    ESP_LOGI(
        kTag,
        "Latest delivery id=%lu outcome=%s",
        static_cast<unsigned long>(receipt.logical_message_id),
        receipt.outcome == messaging::DeliveryOutcome::Delivered
            ? "delivered"
            : "failed");
}

void CommunicatorApp::handle_input(const board::InputState& input)
{
    switch (state_) {
        case State::Main:
            if (options_active_) {
                handle_options_input(input);
            } else {
                handle_main_input(input);
            }
            break;
        case State::IncomingPreset:
            handle_incoming_preset_input(input);
            break;
        case State::ChoosingResponse:
            handle_response_choice_input(input);
            break;
        case State::IncomingResponse:
            handle_incoming_response_input(input);
            break;
        case State::WaitDecision:
            handle_wait_decision_input(input);
            break;
    }
}

void CommunicatorApp::handle_main_input(const board::InputState& input)
{
    const bool peer_known = messaging_.peer_known();
    const std::uint8_t signal_index =
        static_cast<std::uint8_t>(catalogue::kPresetOrder.size());
    const std::uint8_t options_index =
        static_cast<std::uint8_t>(signal_index + 1U);
    const std::uint8_t return_index =
        static_cast<std::uint8_t>(options_index + 1U);
    const std::uint8_t choice_count =
        static_cast<std::uint8_t>(return_index + 1U);

    if (signal_unavailable_feedback_) {
        if (input.primary_short || input.secondary_short) {
            signal_unavailable_feedback_ = false;
            render_main();
        }
        return;
    }

    if (input.secondary_short) {
        selected_main_index_ = static_cast<std::uint8_t>(
            (selected_main_index_ + 1U) % choice_count);
        render_main();
        return;
    }

    if (!input.primary_short) {
        return;
    }

    if (selected_main_index_ == options_index) {
        options_active_ = true;
        selected_options_index_ = 0;
        radio_mode_change_failed_ = false;
        render_options();
        return;
    }

    if (selected_main_index_ == return_index) {
        foreground_exit_requested_ = true;
        return;
    }

    if (selected_main_index_ == signal_index) {
        if (peer_known) {
            (void)send_signal();
        } else {
            signal_unavailable_feedback_ = true;
            render_signal_unavailable();
        }
        return;
    }

    if (!peer_known) {
        return;
    }

    (void)send_selected_preset();
}

void CommunicatorApp::handle_options_input(
    const board::InputState& input)
{
    if (input.secondary_short) {
        selected_options_index_ =
            static_cast<std::uint8_t>((selected_options_index_ + 1U) % 2U);
        radio_mode_change_failed_ = false;
        render_options();
        return;
    }

    if (!input.primary_short) {
        return;
    }

    if (selected_options_index_ == 1U) {
        options_active_ = false;
        radio_mode_change_failed_ = false;
        render_main();
        return;
    }

    const radio::Mode current = messaging_.radio_mode();
    const radio::Mode target =
        current == radio::Mode::Normal
            ? radio::Mode::Lr
            : radio::Mode::Normal;

    if (messaging_.set_radio_mode(target)) {
        radio_mode_change_failed_ = false;
    } else {
        radio_mode_change_failed_ = true;
    }

    render_options();
}

void CommunicatorApp::handle_incoming_preset_input(
    const board::InputState& input)
{
    if (input.primary_short) {
        state_ = State::ChoosingResponse;
        selected_response_index_ = 0;
        render_response_choices();
        return;
    }

    if (input.secondary_short) {
        state_ = State::Main;
        render_main();
    }
}

void CommunicatorApp::handle_response_choice_input(
    const board::InputState& input)
{
    if (input.secondary_short && response_set_.count > 0) {
        selected_response_index_ = static_cast<std::uint8_t>(
            (selected_response_index_ + 1U) % response_set_.count);
        render_response_choices();
        return;
    }

    if (input.primary_short) {
        (void)send_selected_response();
    }
}

void CommunicatorApp::handle_incoming_response_input(
    const board::InputState& input)
{
    if (input.primary_short || input.secondary_short) {
        state_ = State::Main;
        render_main();
    }
}

void CommunicatorApp::handle_wait_decision_input(
    const board::InputState& input)
{
    if (input.primary_short) {
        (void)send_wait_followup();
        return;
    }

    if (input.secondary_short) {
        state_ = State::Main;
        render_main();
    }
}

bool CommunicatorApp::send_selected_preset()
{
    if (!messaging_.peer_known()
        || selected_main_index_ >= catalogue::kPresetOrder.size()) {
        return false;
    }

    const catalogue::PresetId preset =
        catalogue::kPresetOrder[selected_main_index_];

    if (!messaging_.send_preset_message(
            static_cast<std::uint16_t>(preset))) {
        return false;
    }

    track_latest_send();
    state_ = State::Main;
    render_main();
    return true;
}

bool CommunicatorApp::send_signal()
{
    if (!messaging_.peer_known() || !messaging_.send_ring()) {
        return false;
    }

    track_latest_send();
    return true;
}

bool CommunicatorApp::send_selected_response()
{
    if (selected_response_index_ >= response_set_.count
        || !messaging_.peer_known()) {
        return false;
    }

    const catalogue::ResponseId response =
        response_set_.ids[selected_response_index_];

    if (!messaging_.send_preset_response(
            static_cast<std::uint16_t>(incoming_preset_),
            static_cast<std::uint16_t>(response))) {
        return false;
    }

    track_latest_send();
    state_ = State::Main;
    render_main();
    return true;
}

bool CommunicatorApp::send_wait_followup()
{
    if (!messaging_.peer_known()
        || !messaging_.send_preset_message(
            static_cast<std::uint16_t>(catalogue::PresetId::Wait))) {
        return false;
    }

    track_latest_send();
    state_ = State::Main;
    render_main();
    return true;
}

void CommunicatorApp::track_latest_send()
{
    latest_outgoing_message_id_ =
        messaging_.outgoing_logical_message_id();
    latest_delivery_feedback_ = DeliveryFeedback::Sending;
    delivery_result_started_ms_ = 0;
}

void CommunicatorApp::notify_incoming()
{
    signal_sound_.stop();
    display_lifecycle_.note_visible_activity();
    board_.tone(kNotificationToneHz, kNotificationToneMs);

    if (active_) {
        render_current();
    }
}

void CommunicatorApp::start_signal_alert()
{
    signal_return_to_launcher_ = !active_;
    signal_alert_active_ = true;
    signal_audio_complete_rendered_ = false;
    signal_animation_wide_ = false;
    signal_playback_cycles_started_ = 1;

    const std::uint32_t now = now_ms();
    signal_last_animation_ms_ = now;

    display_lifecycle_.note_visible_activity();
    signal_sound_.play_selected();
    render_signal_alert(signal_animation_wide_);
}

void CommunicatorApp::update_signal_alert(std::uint32_t now)
{
    if (!signal_alert_active_) {
        return;
    }

    if (signal_sound_.playing()) {
        if (now - signal_last_animation_ms_ >= kSignalAnimationMs) {
            signal_last_animation_ms_ = now;
            signal_animation_wide_ = !signal_animation_wide_;
            render_signal_alert(signal_animation_wide_);
        }
        return;
    }

    if (signal_playback_cycles_started_ < kSignalPlaybackCount) {
        ++signal_playback_cycles_started_;
        signal_sound_.play_selected();
        return;
    }

    if (!signal_audio_complete_rendered_) {
        signal_audio_complete_rendered_ = true;
        signal_animation_wide_ = false;
        render_signal_alert(false);
    }
}

void CommunicatorApp::dismiss_signal_alert()
{
    signal_sound_.stop();
    signal_alert_active_ = false;
    signal_audio_complete_rendered_ = false;
    signal_animation_wide_ = false;
    signal_playback_cycles_started_ = 0;

    const bool return_to_launcher = signal_return_to_launcher_;
    signal_return_to_launcher_ = false;

    if (!return_to_launcher) {
        render_current();
    }
}

bool CommunicatorApp::any_user_button(
    const board::InputState& input) const
{
    return input.primary_short
        || input.primary_long
        || input.secondary_short
        || input.secondary_long;
}

std::uint8_t CommunicatorApp::signal_bars() const
{
    if (!messaging_.peer_reachable()) {
        return 0;
    }

    std::int8_t rssi = 0;
    if (!messaging_.latest_peer_rssi(rssi)) {
        return 0;
    }

    if (rssi >= -60) {
        return 3;
    }
    if (rssi >= -75) {
        return 2;
    }
    return 1;
}

void CommunicatorApp::render_current()
{
    switch (state_) {
        case State::Main:
            if (options_active_) {
                render_options();
            } else {
                render_main();
            }
            break;
        case State::IncomingPreset:
            render_incoming_preset();
            break;
        case State::ChoosingResponse:
            render_response_choices();
            break;
        case State::IncomingResponse:
            render_incoming_response();
            break;
        case State::WaitDecision:
            render_wait_decision();
            break;
    }
}

void CommunicatorApp::render_main()
{
    signal_unavailable_feedback_ = false;
    clear_screen();
    draw_header("KOMUNIKATOR");

    const bool peer_known = messaging_.peer_known();
    const bool recently_seen = messaging_.peer_reachable();
    const std::uint8_t bars = signal_bars();
    const std::size_t signal_index = catalogue::kPresetOrder.size();
    const std::size_t options_index = signal_index + 1U;
    const std::size_t return_index = options_index + 1U;

    board_.draw_text_region(
        10,
        22,
        82,
        11,
        peer_label_,
        1,
        board::DisplayColor::SecondaryText,
        board::DisplayColor::Background);

    board_.draw_text_region(
        94,
        22,
        96,
        11,
        !peer_known
            ? "SZUKAM..."
            : (recently_seen ? "DOSTEPNY" : "GOTOWY"),
        1,
        recently_seen
            ? board::DisplayColor::StatusActive
            : board::DisplayColor::SecondaryText,
        board::DisplayColor::Background);

    draw_signal_bars(bars, recently_seen);
    board_.draw_line(
        8,
        34,
        231,
        34,
        board::DisplayColor::SecondaryText);

    const bool preset_focused = selected_main_index_ < signal_index;
    const std::size_t displayed_preset_index =
        preset_focused
            ? selected_main_index_
            : signal_index - 1U;
    const catalogue::PresetId displayed_preset =
        catalogue::kPresetOrder[displayed_preset_index];

    board_.draw_text_region(
        8,
        38,
        224,
        48,
        "",
        1,
        board::DisplayColor::PrimaryText,
        board::DisplayColor::Surface);

    board_.draw_text_region(
        12,
        49,
        216,
        30,
        catalogue::preset_text(displayed_preset),
        preset_text_scale(displayed_preset),
        preset_focused && peer_known
            ? board::DisplayColor::PrimaryText
            : board::DisplayColor::SecondaryText,
        board::DisplayColor::Surface);

    if (preset_focused) {
        draw_selection_marker(
            board_,
            8,
            38,
            48,
            peer_known
                ? board::DisplayColor::Accent
                : board::DisplayColor::SecondaryText);
    }

    constexpr const char* kPresetCounters[] = {
        "1 / 5", "2 / 5", "3 / 5", "4 / 5", "5 / 5",
    };
    board_.draw_text_region(
        103,
        88,
        40,
        11,
        kPresetCounters[displayed_preset_index],
        1,
        board::DisplayColor::SecondaryText,
        board::DisplayColor::Background);

    const bool signal_selected =
        selected_main_index_ == signal_index;
    const bool options_selected =
        selected_main_index_ == options_index;
    const bool return_selected =
        selected_main_index_ == return_index;

    const board::DisplayColor signal_background =
        signal_selected
            ? board::DisplayColor::Surface
            : board::DisplayColor::Background;
    const board::DisplayColor signal_color =
        peer_known
            ? board::DisplayColor::Attention
            : board::DisplayColor::SecondaryText;

    board_.draw_text_region(
        8, 100, 86, 20, "", 1, signal_color, signal_background);
    draw_bell_glyph(
        14,
        108,
        1,
        peer_known
            ? board::DisplayColor::Attention
            : board::DisplayColor::SecondaryText);
    board_.draw_text_region(
        22, 102, 72, 18, "SYGNAL", 2, signal_color, signal_background);
    if (signal_selected) {
        draw_selection_marker(
            board_,
            8,
            100,
            19,
            board::DisplayColor::Attention);
    }

    const board::DisplayColor options_background =
        options_selected
            ? board::DisplayColor::Surface
            : board::DisplayColor::Background;
    board_.draw_text_region(
        94,
        100,
        64,
        20,
        "",
        1,
        board::DisplayColor::PrimaryText,
        options_background);
    board_.draw_text_region(
        96,
        102,
        60,
        18,
        "OPCJE",
        2,
        options_selected
            ? board::DisplayColor::PrimaryText
            : board::DisplayColor::SecondaryText,
        options_background);
    if (options_selected) {
        draw_selection_marker(board_, 94, 100, 19);
    }

    const board::DisplayColor return_background =
        return_selected
            ? board::DisplayColor::Surface
            : board::DisplayColor::Background;
    board_.draw_text_region(
        158,
        100,
        74,
        20,
        "",
        1,
        board::DisplayColor::PrimaryText,
        return_background);
    board_.draw_text_region(
        159,
        102,
        72,
        18,
        "POWROT",
        2,
        return_selected
            ? board::DisplayColor::PrimaryText
            : board::DisplayColor::SecondaryText,
        return_background);
    if (return_selected) {
        draw_selection_marker(board_, 158, 100, 19);
    }

    board_.draw_text_region(
        31,
        123,
        190,
        10,
        "M5 WYBIERZ  BOCZNY DALEJ",
        1,
        board::DisplayColor::SecondaryText,
        board::DisplayColor::Background);

    rendered_peer_state_valid_ = true;
    rendered_peer_known_ = peer_known;
    rendered_peer_reachable_ = recently_seen;
    rendered_signal_bars_ = bars;

    render_delivery_feedback();
}

void CommunicatorApp::render_options()
{
    clear_screen();
    draw_header("OPCJE");

    const bool mode_selected = selected_options_index_ == 0U;
    const bool return_selected = selected_options_index_ == 1U;
    const char* mode_text =
        messaging_.radio_mode() == radio::Mode::Lr
            ? "LR"
            : "STANDARD";

    const board::DisplayColor mode_background =
        mode_selected
            ? board::DisplayColor::Surface
            : board::DisplayColor::Background;

    board_.draw_text_region(
        8,
        29,
        224,
        44,
        "",
        1,
        board::DisplayColor::PrimaryText,
        mode_background);

    board_.draw_text_region(
        16,
        30,
        208,
        21,
        "TRYB RADIO",
        2,
        mode_selected
            ? board::DisplayColor::PrimaryText
            : board::DisplayColor::SecondaryText,
        mode_background);

    board_.draw_text_region(
        16,
        51,
        208,
        21,
        mode_text,
        2,
        board::DisplayColor::PrimaryText,
        mode_background);

    if (mode_selected) {
        draw_selection_marker(board_, 8, 30, 40);
    }

    board_.draw_text_region(
        12,
        78,
        216,
        22,
        "POWROT",
        2,
        return_selected
            ? board::DisplayColor::PrimaryText
            : board::DisplayColor::SecondaryText,
        return_selected
            ? board::DisplayColor::Surface
            : board::DisplayColor::Background);

    if (return_selected) {
        draw_selection_marker(board_, 8, 79, 18);
    }

    board_.draw_text_region(
        18,
        102,
        204,
        13,
        radio_mode_change_failed_
            ? "NIE UDALO SIE"
            : "USTAW TAK SAMO NA OBU",
        1,
        radio_mode_change_failed_
            ? board::DisplayColor::Danger
            : board::DisplayColor::SecondaryText,
        board::DisplayColor::Background);

    board_.draw_text_region(
        8,
        120,
        224,
        13,
        "M5 WYBIERZ | BOCZNY DALEJ",
        1,
        board::DisplayColor::SecondaryText,
        board::DisplayColor::Background);

    render_delivery_feedback();
}

void CommunicatorApp::render_main_if_status_changed()
{
    const bool peer_known = messaging_.peer_known();
    const bool recently_seen = messaging_.peer_reachable();
    const std::uint8_t bars = signal_bars();

    if (signal_unavailable_feedback_) {
        if (peer_known) {
            render_main();
        }
        return;
    }

    if (!rendered_peer_state_valid_
        || peer_known != rendered_peer_known_
        || recently_seen != rendered_peer_reachable_
        || bars != rendered_signal_bars_) {
        render_main();
    }
}

void CommunicatorApp::render_incoming_preset()
{
    clear_screen();

    board_.draw_text_region(
        8,
        18,
        224,
        70,
        "",
        1,
        board::DisplayColor::PrimaryText,
        board::DisplayColor::Surface);
    board_.draw_text_region(
        14,
        38,
        212,
        36,
        catalogue::preset_text(incoming_preset_),
        preset_text_scale(incoming_preset_),
        board::DisplayColor::PrimaryText,
        board::DisplayColor::Surface);
    board_.draw_line(
        8,
        18,
        8,
        87,
        board::DisplayColor::Accent);

    board_.draw_text_region(
        14,
        108,
        104,
        16,
        "M5 ODPOWIEDZ",
        1,
        board::DisplayColor::PrimaryText,
        board::DisplayColor::Background);
    board_.draw_text_region(
        132,
        108,
        96,
        16,
        "BOCZNY POMIN",
        1,
        board::DisplayColor::SecondaryText,
        board::DisplayColor::Background);
}

void CommunicatorApp::render_response_choices()
{
    clear_screen();

    board_.draw_text_region(
        12,
        8,
        216,
        20,
        catalogue::preset_text(incoming_preset_),
        context_text_scale(incoming_preset_),
        board::DisplayColor::SecondaryText,
        board::DisplayColor::Background);

    const catalogue::ResponseId response =
        response_set_.ids[selected_response_index_];
    board_.draw_text_region(
        8,
        34,
        224,
        60,
        "",
        1,
        board::DisplayColor::PrimaryText,
        board::DisplayColor::Surface);
    board_.draw_text_region(
        14,
        50,
        212,
        32,
        catalogue::response_text(response),
        response_text_scale(response),
        board::DisplayColor::PrimaryText,
        board::DisplayColor::Surface);
    draw_selection_marker(
        board_,
        8,
        34,
        60);

    board_.draw_text_region(
        102,
        97,
        46,
        10,
        choice_counter(selected_response_index_, response_set_.count),
        1,
        board::DisplayColor::SecondaryText,
        board::DisplayColor::Background);

    board_.draw_text_region(
        27,
        118,
        202,
        14,
        "M5 WYBIERZ  BOCZNY DALEJ",
        1,
        board::DisplayColor::SecondaryText,
        board::DisplayColor::Background);
}

void CommunicatorApp::render_incoming_response()
{
    clear_screen();

    board_.draw_text_region(
        12,
        8,
        216,
        20,
        catalogue::preset_text(incoming_preset_),
        context_text_scale(incoming_preset_),
        board::DisplayColor::SecondaryText,
        board::DisplayColor::Background);

    board_.draw_text_region(
        8,
        34,
        224,
        62,
        "",
        1,
        board::DisplayColor::PrimaryText,
        board::DisplayColor::Surface);
    board_.draw_text_region(
        14,
        50,
        212,
        34,
        catalogue::response_text(incoming_response_),
        response_text_scale(incoming_response_),
        board::DisplayColor::PrimaryText,
        board::DisplayColor::Surface);
    board_.draw_line(
        8,
        34,
        8,
        95,
        board::DisplayColor::Accent);

    board_.draw_text_region(
        45,
        116,
        186,
        16,
        "M5 / BOCZNY = ZAMKNIJ",
        1,
        board::DisplayColor::SecondaryText,
        board::DisplayColor::Background);
}

void CommunicatorApp::render_wait_decision()
{
    clear_screen();

    board_.draw_text_region(
        12,
        7,
        216,
        20,
        catalogue::preset_text(incoming_preset_),
        context_text_scale(incoming_preset_),
        board::DisplayColor::SecondaryText,
        board::DisplayColor::Background);

    board_.draw_text_region(
        8,
        31,
        224,
        50,
        "",
        1,
        board::DisplayColor::PrimaryText,
        board::DisplayColor::Surface);
    board_.draw_text_region(
        14,
        42,
        212,
        30,
        catalogue::response_text(incoming_response_),
        response_text_scale(incoming_response_),
        board::DisplayColor::PrimaryText,
        board::DisplayColor::Surface);
    board_.draw_line(
        8,
        31,
        8,
        80,
        board::DisplayColor::Accent);

    board_.draw_text_region(
        28,
        87,
        195,
        20,
        "M5 CZEKAC?",
        2,
        board::DisplayColor::Accent,
        board::DisplayColor::Background);
    board_.draw_text_region(
        28,
        113,
        195,
        16,
        "BOCZNY ZAMKNIJ",
        1,
        board::DisplayColor::SecondaryText,
        board::DisplayColor::Background);
}

void CommunicatorApp::render_delivery_feedback()
{
    if (latest_delivery_feedback_ == DeliveryFeedback::None
        || !delivery_feedback_overlay_allowed()) {
        return;
    }

    const char* text = "WYSYLAM...";
    board::DisplayColor color = board::DisplayColor::Accent;

    if (latest_delivery_feedback_ == DeliveryFeedback::Delivered) {
        text = "DOSTARCZONO";
        color = board::DisplayColor::StatusActive;
    } else if (latest_delivery_feedback_ == DeliveryFeedback::Failed) {
        text = "NIE DOSTARCZONO";
        color = board::DisplayColor::Danger;
    }

    board_.draw_text_region(
        50,
        120,
        140,
        15,
        "",
        1,
        color,
        board::DisplayColor::Surface);
    board_.draw_line(50, 120, 189, 120, color);
    board_.draw_text_region(
        61,
        123,
        118,
        10,
        text,
        1,
        color,
        board::DisplayColor::Surface);
}

void CommunicatorApp::render_signal_unavailable()
{
    clear_screen();
    draw_header("SYGNAL");

    draw_bell_glyph(
        120,
        48,
        2,
        board::DisplayColor::SecondaryText);

    board_.draw_text_region(
        29,
        70,
        190,
        20,
        "BRAK POLACZENIA",
        2,
        board::DisplayColor::PrimaryText,
        board::DisplayColor::Background);
    board_.draw_text_region(
        55,
        96,
        160,
        12,
        "SZUKAM DRUGIEGO M5...",
        1,
        board::DisplayColor::SecondaryText,
        board::DisplayColor::Background);
    board_.draw_text_region(
        50,
        120,
        175,
        12,
        "M5 / BOCZNY = POWROT",
        1,
        board::DisplayColor::SecondaryText,
        board::DisplayColor::Background);
}

void CommunicatorApp::render_signal_alert(bool wide_arcs)
{
    clear_screen();

    draw_ringing_arcs(wide_arcs);
    draw_bell_glyph(
        120,
        55,
        3,
        board::DisplayColor::Attention);

    board_.draw_text_region(
        47,
        88,
        146,
        30,
        "SYGNAL",
        3,
        board::DisplayColor::Attention,
        board::DisplayColor::Background);

    board_.draw_text_region(
        34,
        120,
        172,
        12,
        "DOWOLNY PRZYCISK = ZAMKNIJ",
        1,
        board::DisplayColor::SecondaryText,
        board::DisplayColor::Background);
}

void CommunicatorApp::draw_bell_glyph(
    std::int16_t center_x,
    std::int16_t center_y,
    std::int16_t scale,
    board::DisplayColor color)
{
    const std::int16_t half =
        static_cast<std::int16_t>(4 * scale);
    const std::int16_t top =
        static_cast<std::int16_t>(center_y - 5 * scale);
    const std::int16_t shoulder =
        static_cast<std::int16_t>(center_y - 3 * scale);
    const std::int16_t bottom =
        static_cast<std::int16_t>(center_y + 4 * scale);

    board_.draw_line(
        center_x,
        static_cast<std::int16_t>(top - scale),
        center_x,
        top,
        color);
    board_.draw_line(
        static_cast<std::int16_t>(center_x - 2 * scale),
        shoulder,
        static_cast<std::int16_t>(center_x - half),
        bottom,
        color);
    board_.draw_line(
        static_cast<std::int16_t>(center_x + 2 * scale),
        shoulder,
        static_cast<std::int16_t>(center_x + half),
        bottom,
        color);
    board_.draw_line(
        static_cast<std::int16_t>(center_x - 2 * scale),
        shoulder,
        static_cast<std::int16_t>(center_x + 2 * scale),
        shoulder,
        color);
    board_.draw_line(
        static_cast<std::int16_t>(center_x - half - scale),
        bottom,
        static_cast<std::int16_t>(center_x + half + scale),
        bottom,
        color);
    board_.fill_circle(
        center_x,
        static_cast<std::int16_t>(bottom + 2 * scale),
        scale,
        color);
}

void CommunicatorApp::draw_ringing_arcs(bool wide_arcs)
{
    const std::int16_t offset = wide_arcs ? 42 : 34;
    const std::int16_t upper_y = wide_arcs ? 34 : 38;
    const std::int16_t lower_y = wide_arcs ? 72 : 68;

    board_.draw_line(
        static_cast<std::int16_t>(120 - offset),
        upper_y,
        static_cast<std::int16_t>(120 - offset - 7),
        48,
        board::DisplayColor::Attention);
    board_.draw_line(
        static_cast<std::int16_t>(120 - offset - 7),
        48,
        static_cast<std::int16_t>(120 - offset),
        lower_y,
        board::DisplayColor::Attention);

    board_.draw_line(
        static_cast<std::int16_t>(120 + offset),
        upper_y,
        static_cast<std::int16_t>(120 + offset + 7),
        48,
        board::DisplayColor::Attention);
    board_.draw_line(
        static_cast<std::int16_t>(120 + offset + 7),
        48,
        static_cast<std::int16_t>(120 + offset),
        lower_y,
        board::DisplayColor::Attention);
}

void CommunicatorApp::clear_screen()
{
    board_.draw_text_region(
        0,
        0,
        kScreenWidth,
        kScreenHeight,
        "",
        1,
        board::DisplayColor::PrimaryText,
        board::DisplayColor::Background);
}

void CommunicatorApp::draw_header(const char* title)
{
    board_.draw_text_region(
        8,
        3,
        224,
        18,
        title,
        2,
        board::DisplayColor::PrimaryText,
        board::DisplayColor::Background);
}

void CommunicatorApp::draw_signal_bars(
    std::uint8_t bars,
    bool reachable)
{
    constexpr std::int16_t base_y = 31;
    constexpr std::int16_t start_x = 202;
    constexpr std::int16_t heights[] = {3, 6, 9};

    for (std::uint8_t index = 0; index < 3; ++index) {
        const bool active_bar = reachable && index < bars;
        const board::DisplayColor color =
            active_bar
                ? board::DisplayColor::StatusActive
                : board::DisplayColor::SecondaryText;
        const std::int16_t x =
            static_cast<std::int16_t>(start_x + index * 10);
        const std::int16_t top =
            static_cast<std::int16_t>(base_y - heights[index]);

        for (std::int16_t dx = 0; dx < 4; ++dx) {
            board_.draw_line(
                static_cast<std::int16_t>(x + dx),
                top,
                static_cast<std::int16_t>(x + dx),
                base_y,
                color);
        }
    }
}

}  // namespace nikos::communicator
