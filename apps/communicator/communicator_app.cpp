#include "communicator/communicator_app.hpp"

#include <algorithm>
#include <cstddef>

#include "esp_timer.h"

namespace {

constexpr std::uint32_t kNotificationToneMs = 90;
constexpr float kNotificationToneHz = 2600.0F;

constexpr std::uint32_t kSignalToneMs = 180;
constexpr std::uint32_t kSignalShortSilenceMs = 180;
constexpr std::uint32_t kSignalLongSilenceMs = 500;
constexpr std::uint8_t kSignalStepsPerRepeat = 4;
constexpr std::uint8_t kSignalRepeatCount = 3;
constexpr std::uint8_t kSignalStepCount =
    kSignalStepsPerRepeat * kSignalRepeatCount;
constexpr std::uint32_t kSignalAnimationMs = 120;
constexpr float kSignalToneOneHz = 2400.0F;
constexpr float kSignalToneTwoHz = 2800.0F;

constexpr std::int16_t kScreenWidth = 240;
constexpr std::int16_t kScreenHeight = 135;

std::uint32_t now_ms()
{
    return static_cast<std::uint32_t>(esp_timer_get_time() / 1000U);
}

}  // namespace

namespace nikos::communicator {

CommunicatorApp::CommunicatorApp(
    board::Board& board,
    messaging::Service& messaging,
    const char* peer_label)
    : board_(board),
      messaging_(messaging),
      peer_label_(peer_label)
{
}

bool CommunicatorApp::begin()
{
    active_ = true;
    const bool profile_ok =
        messaging_.set_rx_profile(messaging::RxProfile::Foreground);
    if (!signal_alert_active_) {
        render_current();
    }
    return profile_ok;
}

bool CommunicatorApp::end()
{
    if (signal_alert_active_) {
        board_.stop_tone();
        signal_alert_active_ = false;
        signal_pattern_running_ = false;
        signal_return_to_launcher_ = false;
    }

    active_ = false;

    if (state_ == State::HumanOkReceived) {
        if (suspended_waiting_.valid) {
            restore_suspended_waiting(false);
        } else {
            state_ = State::Main;
        }
    }

    return messaging_.set_rx_profile(messaging::RxProfile::Background);
}

void CommunicatorApp::reset_session()
{
    board_.stop_tone();

    active_ = false;
    state_ = State::Main;

    selected_main_index_ = 0;
    selected_response_index_ = 0;
    selected_wait_decision_index_ = 0;

    sent_preset_ = catalogue::PresetId::Greeting;
    incoming_preset_ = catalogue::PresetId::Greeting;
    incoming_response_ = catalogue::ResponseId::GreetingHello;
    response_set_ = catalogue::ResponseSet{};

    current_incoming_ = messaging::IncomingMessage{};
    deferred_incoming_ = messaging::IncomingMessage{};
    deferred_incoming_valid_ = false;
    suspended_waiting_ = SuspendedWaitingContext{};

    expected_response_reference_ = 0;
    expected_human_ack_reference_ = 0;
    last_greeting_message_id_ = 0;

    signal_alert_active_ = false;
    signal_return_to_launcher_ = false;
    signal_pattern_running_ = false;
    signal_animation_wide_ = false;
    signal_audio_step_ = 0;
    signal_step_started_ms_ = 0;
    signal_last_animation_ms_ = 0;

    rendered_peer_state_valid_ = false;
    rendered_peer_reachable_ = false;
    rendered_signal_bars_ = 0;
}

CommunicatorApp::UpdateResult CommunicatorApp::update()
{
    messaging::DeliveryReceipt receipt;
    (void)messaging_.poll_delivery(receipt);

    if (!signal_alert_active_) {
        (void)process_incoming();
    }

    if (signal_alert_active_) {
        update_signal_alert(now_ms());

        const board::InputState input = board_.poll_input();
        if (any_user_button(input)) {
            const bool return_to_launcher = signal_return_to_launcher_;
            dismiss_signal_alert();
            return return_to_launcher
                ? UpdateResult::ExitRequested
                : UpdateResult::Running;
        }

        return UpdateResult::Running;
    }

    if (state_ == State::Main) {
        render_main_if_status_changed();
    }

    const board::InputState input = board_.poll_input();

    if (input.secondary_long) {
        return UpdateResult::ExitRequested;
    }

    handle_input(input);
    return UpdateResult::Running;
}

bool CommunicatorApp::accept_incoming(const messaging::IncomingMessage& message)
{
    if (message.kind == messaging::IncomingKind::Ring) {
        if (signal_alert_active_) {
            return false;
        }

        start_signal_alert();
        return true;
    }

    if (message.kind == messaging::IncomingKind::PresetMessage) {
        catalogue::PresetId preset;
        if (!catalogue::preset_from_wire(message.value_id, preset)) {
            return false;
        }

        const bool idle_message = state_ == State::Main;
        const bool wait_followup =
            state_ == State::WaitingForHumanAck
            && preset == catalogue::PresetId::Wait;
        const bool collision_yield =
            should_yield_simultaneous_preset(preset);

        if (!idle_message && !wait_followup && !collision_yield) {
            return false;
        }

        if (collision_yield) {
            suspend_waiting_for_response();
        }

        current_incoming_ = message;
        incoming_preset_ = preset;
        response_set_ = catalogue::responses_for(preset);
        selected_response_index_ = 0;
        state_ = State::IncomingPreset;
        notify_incoming();
        return true;
    }

    if (message.kind != messaging::IncomingKind::PresetResponse) {
        return false;
    }

    catalogue::ResponseId response;
    if (!catalogue::response_from_wire(message.value_id, response)) {
        return false;
    }

    if (state_ == State::Main
        && response == catalogue::ResponseId::GreetingHello
        && last_greeting_message_id_ != 0
        && message.reference_message_id == last_greeting_message_id_) {
        current_incoming_ = message;
        incoming_response_ = response;
        last_greeting_message_id_ = 0;
        state_ = State::IncomingResponse;
        notify_incoming();
        return true;
    }

    if (state_ == State::WaitingForHumanAck
        && response == catalogue::ResponseId::HumanOk
        && message.reference_message_id == expected_human_ack_reference_) {
        current_incoming_ = message;
        incoming_response_ = response;
        state_ = State::HumanOkReceived;
        notify_incoming();
        return true;
    }

    const bool waiting_for_response =
        state_ == State::WaitingForResponse
        || state_ == State::WaitingForWaitResponse;

    if (!waiting_for_response
        || message.reference_message_id != expected_response_reference_
        || !catalogue::response_allowed_for(sent_preset_, response)) {
        return false;
    }

    current_incoming_ = message;
    incoming_response_ = response;

    if (state_ == State::WaitingForResponse
        && catalogue::needs_wait_decision(response)) {
        state_ = State::WaitDecision;
        selected_wait_decision_index_ = 0;
    } else {
        state_ = State::IncomingResponse;
    }

    notify_incoming();
    return true;
}

bool CommunicatorApp::process_incoming()
{
    if (deferred_incoming_valid_
        && accept_incoming(deferred_incoming_)) {
        deferred_incoming_valid_ = false;
        return true;
    }

    messaging::IncomingMessage incoming;
    while (messaging_.peek_incoming(incoming)) {
        if (accept_incoming(incoming)) {
            (void)messaging_.consume_incoming(incoming.logical_message_id);
            return true;
        }

        if (!can_defer_incoming(incoming)
            || deferred_incoming_valid_) {
            return false;
        }

        // Retain exactly one temporarily incompatible event locally so it
        // cannot block a later event required by the current exchange.
        deferred_incoming_ = incoming;
        deferred_incoming_valid_ = true;

        if (!messaging_.consume_incoming(incoming.logical_message_id)) {
            deferred_incoming_valid_ = false;
            return false;
        }
    }

    return false;
}

bool CommunicatorApp::can_defer_incoming(
    const messaging::IncomingMessage& message) const
{
    if (message.kind == messaging::IncomingKind::Ring) {
        return true;
    }

    if (message.kind == messaging::IncomingKind::PresetMessage) {
        catalogue::PresetId preset;
        return catalogue::preset_from_wire(message.value_id, preset);
    }

    if (message.kind != messaging::IncomingKind::PresetResponse) {
        return false;
    }

    catalogue::ResponseId response;
    if (!catalogue::response_from_wire(message.value_id, response)) {
        return false;
    }

    if (response == catalogue::ResponseId::GreetingHello) {
        return last_greeting_message_id_ != 0
            && message.reference_message_id == last_greeting_message_id_;
    }

    if (response == catalogue::ResponseId::HumanOk) {
        return expected_human_ack_reference_ != 0
            && message.reference_message_id
                == expected_human_ack_reference_;
    }

    const bool matches_active_wait =
        expected_response_reference_ != 0
        && message.reference_message_id == expected_response_reference_
        && catalogue::response_allowed_for(sent_preset_, response);

    const bool matches_suspended_wait =
        suspended_waiting_.valid
        && message.reference_message_id
            == suspended_waiting_.expected_response_reference
        && catalogue::response_allowed_for(
            suspended_waiting_.sent_preset,
            response);

    return matches_active_wait || matches_suspended_wait;
}

bool CommunicatorApp::should_yield_simultaneous_preset(
    catalogue::PresetId preset) const
{
    if (state_ != State::WaitingForResponse
        || suspended_waiting_.valid
        || preset == catalogue::PresetId::Greeting
        || !messaging_.peer_known()) {
        return false;
    }

    // Both peers know the same pair of MACs. Exactly the lower self MAC yields.
    return messaging_.self_mac() < messaging_.peer_mac();
}

void CommunicatorApp::suspend_waiting_for_response()
{
    suspended_waiting_.valid = true;
    suspended_waiting_.sent_preset = sent_preset_;
    suspended_waiting_.expected_response_reference =
        expected_response_reference_;

    // Recognition of the original expected response moves to the suspended
    // context while the peer's colliding exchange is handled.
    expected_response_reference_ = 0;
}

void CommunicatorApp::restore_suspended_waiting(bool render)
{
    if (!suspended_waiting_.valid) {
        state_ = State::Main;
        if (render) {
            render_main();
        }
        return;
    }

    sent_preset_ = suspended_waiting_.sent_preset;
    expected_response_reference_ =
        suspended_waiting_.expected_response_reference;
    expected_human_ack_reference_ = 0;
    suspended_waiting_ = SuspendedWaitingContext{};
    state_ = State::WaitingForResponse;

    if (render) {
        render_waiting_for_response();
    }
}

void CommunicatorApp::handle_input(const board::InputState& input)
{
    switch (state_) {
        case State::Main:
            handle_main_input(input);
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
        case State::HumanOkReceived:
            handle_human_ok_input(input);
            break;
        case State::WaitingForResponse:
        case State::WaitingForHumanAck:
        case State::WaitingForWaitResponse:
            break;
    }
}

void CommunicatorApp::handle_main_input(const board::InputState& input)
{
    const bool reachable = messaging_.peer_reachable();
    const std::uint8_t choice_count = static_cast<std::uint8_t>(
        catalogue::kPresetOrder.size() + (reachable ? 1U : 0U));

    if (input.secondary_short) {
        selected_main_index_ = static_cast<std::uint8_t>(
            (selected_main_index_ + 1U) % choice_count);
        render_main();
        return;
    }

    if (!input.primary_short || !reachable) {
        return;
    }

    if (selected_main_index_ == catalogue::kPresetOrder.size()) {
        (void)send_signal();
        return;
    }

    (void)send_selected_preset();
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

    if (input.secondary_short
        && incoming_preset_ == catalogue::PresetId::Greeting) {
        state_ = State::Main;
        render_main();
    }
}

void CommunicatorApp::handle_response_choice_input(
    const board::InputState& input)
{
    if (input.secondary_short && response_set_.count > 1) {
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
    if (incoming_response_ == catalogue::ResponseId::GreetingHello) {
        if (input.primary_short || input.secondary_short) {
            state_ = State::Main;
            render_main();
        }
        return;
    }

    if (input.primary_short) {
        if (send_human_ok(current_incoming_.logical_message_id)) {
            state_ = State::Main;
            render_main();
        }
    }
}

void CommunicatorApp::handle_wait_decision_input(
    const board::InputState& input)
{
    if (input.secondary_short) {
        selected_wait_decision_index_ =
            static_cast<std::uint8_t>((selected_wait_decision_index_ + 1U) % 2U);
        render_wait_decision();
        return;
    }

    if (!input.primary_short) {
        return;
    }

    if (selected_wait_decision_index_ == 0) {
        if (send_human_ok(current_incoming_.logical_message_id)) {
            state_ = State::Main;
            render_main();
        }
        return;
    }

    (void)send_wait_followup();
}

void CommunicatorApp::handle_human_ok_input(const board::InputState& input)
{
    if (input.primary_short || input.secondary_short) {
        if (suspended_waiting_.valid) {
            restore_suspended_waiting(true);
        } else {
            state_ = State::Main;
            render_main();
        }
    }
}

bool CommunicatorApp::send_selected_preset()
{
    if (!messaging_.peer_reachable()) {
        return false;
    }

    if (selected_main_index_ >= catalogue::kPresetOrder.size()) {
        return false;
    }

    const catalogue::PresetId preset =
        catalogue::kPresetOrder[selected_main_index_];

    if (!messaging_.send_preset_message(
            static_cast<std::uint16_t>(preset))) {
        return false;
    }

    sent_preset_ = preset;
    const std::uint32_t logical_message_id =
        messaging_.outgoing_logical_message_id();

    if (preset == catalogue::PresetId::Greeting) {
        last_greeting_message_id_ = logical_message_id;
        state_ = State::Main;
        render_main();
        return true;
    }

    expected_response_reference_ = logical_message_id;
    state_ = State::WaitingForResponse;
    render_waiting_for_response();
    return true;
}

bool CommunicatorApp::send_signal()
{
    if (!messaging_.peer_reachable()) {
        return false;
    }

    // SYGNAŁ is intentionally outside the preset catalogue and conversation
    // state machine. Delivery still uses messaging RING retry/ACK/dedupe.
    return messaging_.send_ring();
}

bool CommunicatorApp::send_selected_response()
{
    if (selected_response_index_ >= response_set_.count
        || !messaging_.peer_reachable()) {
        return false;
    }

    const catalogue::ResponseId response =
        response_set_.ids[selected_response_index_];

    if (!messaging_.send_preset_response(
            static_cast<std::uint16_t>(response),
            current_incoming_.logical_message_id)) {
        return false;
    }

    if (incoming_preset_ == catalogue::PresetId::Greeting) {
        state_ = State::Main;
        render_main();
        return true;
    }

    expected_human_ack_reference_ =
        messaging_.outgoing_logical_message_id();
    state_ = State::WaitingForHumanAck;
    render_waiting_for_human_ack();
    return true;
}

bool CommunicatorApp::send_human_ok(std::uint32_t reference_message_id)
{
    if (!messaging_.peer_reachable()) {
        return false;
    }

    return messaging_.send_preset_response(
        static_cast<std::uint16_t>(catalogue::ResponseId::HumanOk),
        reference_message_id);
}

bool CommunicatorApp::send_wait_followup()
{
    if (!messaging_.peer_reachable()
        || !messaging_.send_preset_message(
            static_cast<std::uint16_t>(catalogue::PresetId::Wait))) {
        return false;
    }

    sent_preset_ = catalogue::PresetId::Wait;
    expected_response_reference_ =
        messaging_.outgoing_logical_message_id();
    state_ = State::WaitingForWaitResponse;
    render_waiting_for_response();
    return true;
}

void CommunicatorApp::notify_incoming()
{
    board_.wake_display();
    board_.tone(kNotificationToneHz, kNotificationToneMs);

    if (active_) {
        render_current();
    }
}

void CommunicatorApp::start_signal_alert()
{
    signal_return_to_launcher_ = !active_;
    signal_alert_active_ = true;
    signal_pattern_running_ = true;
    signal_animation_wide_ = false;
    signal_audio_step_ = 0;

    const std::uint32_t now = now_ms();
    signal_step_started_ms_ = now;
    signal_last_animation_ms_ = now;

    board_.wake_display();
    board_.stop_tone();
    board_.tone(kSignalToneOneHz, kSignalToneMs);
    render_signal_alert(signal_animation_wide_);
}

void CommunicatorApp::update_signal_alert(std::uint32_t now)
{
    if (!signal_alert_active_) {
        return;
    }

    if (signal_pattern_running_) {
        while (signal_pattern_running_) {
            const std::uint8_t phase =
                static_cast<std::uint8_t>(
                    signal_audio_step_ % kSignalStepsPerRepeat);
            const std::uint32_t duration =
                phase == 3
                    ? kSignalLongSilenceMs
                    : kSignalToneMs;

            if (phase == 1) {
                // The short silent segment has the same 180 ms duration.
                if (now - signal_step_started_ms_ < kSignalShortSilenceMs) {
                    break;
                }
            } else if (now - signal_step_started_ms_ < duration) {
                break;
            }

            signal_step_started_ms_ +=
                phase == 1 ? kSignalShortSilenceMs : duration;
            ++signal_audio_step_;

            if (signal_audio_step_ >= kSignalStepCount) {
                signal_pattern_running_ = false;
                board_.stop_tone();
                render_signal_alert(false);
                break;
            }

            advance_signal_audio_step(now);
        }
    }

    if (signal_pattern_running_
        && now - signal_last_animation_ms_ >= kSignalAnimationMs) {
        signal_last_animation_ms_ = now;
        signal_animation_wide_ = !signal_animation_wide_;
        render_signal_alert(signal_animation_wide_);
    }
}

void CommunicatorApp::advance_signal_audio_step(std::uint32_t)
{
    const std::uint8_t phase =
        static_cast<std::uint8_t>(
            signal_audio_step_ % kSignalStepsPerRepeat);

    switch (phase) {
        case 0:
            board_.tone(kSignalToneOneHz, kSignalToneMs);
            break;
        case 1:
            board_.stop_tone();
            break;
        case 2:
            board_.tone(kSignalToneTwoHz, kSignalToneMs);
            break;
        case 3:
        default:
            board_.stop_tone();
            break;
    }
}

void CommunicatorApp::dismiss_signal_alert()
{
    board_.stop_tone();
    signal_alert_active_ = false;
    signal_pattern_running_ = false;
    signal_audio_step_ = 0;
    signal_animation_wide_ = false;

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
            render_main();
            break;
        case State::WaitingForResponse:
        case State::WaitingForWaitResponse:
            render_waiting_for_response();
            break;
        case State::IncomingPreset:
            render_incoming_preset();
            break;
        case State::ChoosingResponse:
            render_response_choices();
            break;
        case State::WaitingForHumanAck:
            render_waiting_for_human_ack();
            break;
        case State::IncomingResponse:
            render_incoming_response();
            break;
        case State::WaitDecision:
            render_wait_decision();
            break;
        case State::HumanOkReceived:
            render_human_ok_received();
            break;
    }
}

void CommunicatorApp::render_main()
{
    clear_screen();
    draw_header("KOMUNIKATOR");

    const bool reachable = messaging_.peer_reachable();
    const std::uint8_t bars = signal_bars();
    const std::size_t signal_index = catalogue::kPresetOrder.size();

    if (!reachable && selected_main_index_ >= signal_index) {
        selected_main_index_ = 0;
    }

    board_.draw_polish_ui_text_region(
        10,
        20,
        145,
        14,
        peer_label_,
        1,
        reachable
            ? board::DisplayColor::Ivory
            : board::DisplayColor::MutedBlue,
        board::DisplayColor::Navy);

    board_.draw_polish_ui_text_region(
        10,
        33,
        150,
        12,
        reachable ? u8"DOSTĘPNY" : u8"NIEDOSTĘPNY",
        1,
        reachable
            ? board::DisplayColor::AccentGreen
            : board::DisplayColor::MutedBlue,
        board::DisplayColor::Navy);

    draw_signal_bars(bars, reachable);
    board_.draw_line(
        8,
        47,
        231,
        47,
        board::DisplayColor::MutedBlue);

    const std::size_t selected_preset =
        selected_main_index_ < signal_index
            ? selected_main_index_
            : signal_index - 1U;
    std::size_t first = selected_preset > 0
        ? selected_preset - 1U
        : 0U;
    const std::size_t max_first = catalogue::kPresetOrder.size() - 2U;
    first = std::min(first, max_first);

    for (std::size_t slot = 0; slot < 2; ++slot) {
        const std::size_t index = first + slot;
        const bool selected_row =
            reachable && selected_main_index_ == index;
        const std::int16_t y =
            static_cast<std::int16_t>(52 + slot * 18);

        board_.draw_polish_ui_text_region(
            12,
            y,
            216,
            15,
            catalogue::preset_text(catalogue::kPresetOrder[index]),
            1,
            selected_row
                ? board::DisplayColor::Ivory
                : board::DisplayColor::MutedBlue,
            selected_row
                ? board::DisplayColor::PanelNavy
                : board::DisplayColor::Navy);

        if (selected_row) {
            board_.draw_line(
                8,
                y,
                8,
                static_cast<std::int16_t>(y + 12),
                board::DisplayColor::AccentGreen);
        }
    }

    board_.draw_line(
        8,
        88,
        231,
        88,
        board::DisplayColor::MutedBlue);

    const bool signal_selected =
        reachable && selected_main_index_ == signal_index;
    const board::DisplayColor signal_color =
        reachable
            ? board::DisplayColor::Orange
            : board::DisplayColor::MutedBlue;

    draw_bell_glyph(
        24,
        101,
        1,
        signal_color);

    board_.draw_polish_ui_text_region(
        44,
        91,
        150,
        22,
        u8"SYGNAŁ",
        2,
        signal_color,
        signal_selected
            ? board::DisplayColor::PanelNavy
            : board::DisplayColor::Navy);

    if (signal_selected) {
        board_.draw_line(
            8,
            92,
            8,
            109,
            board::DisplayColor::Orange);
    }

    board_.draw_polish_ui_text_region(
        8,
        117,
        224,
        13,
        reachable
            ? u8"M5 WYŚLIJ  |  SIDE DALEJ"
            : u8"BRAK ŁĄCZNOŚCI  |  SIDE DALEJ",
        1,
        board::DisplayColor::MutedBlue,
        board::DisplayColor::Navy);

    rendered_peer_state_valid_ = true;
    rendered_peer_reachable_ = reachable;
    rendered_signal_bars_ = bars;
}

void CommunicatorApp::render_main_if_status_changed()
{
    const bool reachable = messaging_.peer_reachable();
    const std::uint8_t bars = signal_bars();

    if (!rendered_peer_state_valid_
        || reachable != rendered_peer_reachable_
        || bars != rendered_signal_bars_) {
        render_main();
    }
}

void CommunicatorApp::render_waiting_for_response()
{
    clear_screen();
    draw_header("KOMUNIKATOR");

    board_.draw_polish_ui_text_region(
        12,
        34,
        216,
        28,
        catalogue::preset_text(sent_preset_),
        2,
        board::DisplayColor::Ivory,
        board::DisplayColor::Navy);
    board_.draw_polish_ui_text_region(
        12,
        78,
        216,
        18,
        u8"CZEKAM NA ODPOWIEDŹ...",
        1,
        board::DisplayColor::MutedBlue,
        board::DisplayColor::Navy);
    board_.draw_polish_ui_text_region(
        12,
        116,
        216,
        14,
        "SIDE HOLD = MENU",
        1,
        board::DisplayColor::MutedBlue,
        board::DisplayColor::Navy);
}

void CommunicatorApp::render_incoming_preset()
{
    clear_screen();
    draw_header(u8"WIADOMOŚĆ");

    board_.draw_polish_ui_text_region(
        12,
        42,
        216,
        34,
        catalogue::preset_text(incoming_preset_),
        2,
        board::DisplayColor::Ivory,
        board::DisplayColor::PanelNavy);

    const bool can_dismiss =
        incoming_preset_ == catalogue::PresetId::Greeting;

    board_.draw_polish_ui_text_region(
        8,
        112,
        224,
        18,
        can_dismiss
            ? u8"M5 ODPOWIEDŹ  |  SIDE ZAMKNIJ"
            : u8"M5 ODPOWIEDŹ",
        1,
        board::DisplayColor::MutedBlue,
        board::DisplayColor::Navy);
}

void CommunicatorApp::render_response_choices()
{
    clear_screen();
    draw_header(u8"ODPOWIEDŹ");

    board_.draw_polish_ui_text_region(
        10,
        22,
        220,
        14,
        catalogue::preset_text(incoming_preset_),
        1,
        board::DisplayColor::MutedBlue,
        board::DisplayColor::Navy);

    for (std::uint8_t index = 0; index < response_set_.count; ++index) {
        const bool selected = index == selected_response_index_;
        const std::int16_t y =
            static_cast<std::int16_t>(45 + index * 22);

        board_.draw_polish_ui_text_region(
            12,
            y,
            216,
            18,
            catalogue::response_text(response_set_.ids[index]),
            1,
            selected
                ? board::DisplayColor::Ivory
                : board::DisplayColor::MutedBlue,
            selected
                ? board::DisplayColor::PanelNavy
                : board::DisplayColor::Navy);

        if (selected) {
            board_.draw_line(
                8,
                y,
                8,
                static_cast<std::int16_t>(y + 14),
                board::DisplayColor::AccentGreen);
        }
    }

    board_.draw_polish_ui_text_region(
        8,
        116,
        224,
        14,
        u8"M5 WYBIERZ  |  SIDE DALEJ",
        1,
        board::DisplayColor::MutedBlue,
        board::DisplayColor::Navy);
}

void CommunicatorApp::render_waiting_for_human_ack()
{
    clear_screen();
    draw_header("KOMUNIKATOR");

    board_.draw_polish_ui_text_region(
        12,
        42,
        216,
        22,
        u8"ODPOWIEDŹ WYSŁANA",
        1,
        board::DisplayColor::Ivory,
        board::DisplayColor::Navy);
    board_.draw_polish_ui_text_region(
        12,
        70,
        216,
        22,
        "CZEKAM NA OK",
        2,
        board::DisplayColor::AccentGreen,
        board::DisplayColor::Navy);
    board_.draw_polish_ui_text_region(
        12,
        116,
        216,
        14,
        "SIDE HOLD = MENU",
        1,
        board::DisplayColor::MutedBlue,
        board::DisplayColor::Navy);
}

void CommunicatorApp::render_incoming_response()
{
    clear_screen();
    draw_header(u8"ODPOWIEDŹ");

    board_.draw_polish_ui_text_region(
        12,
        45,
        216,
        30,
        catalogue::response_text(incoming_response_),
        1,
        board::DisplayColor::Ivory,
        board::DisplayColor::PanelNavy);
    board_.draw_polish_ui_text_region(
        8,
        112,
        224,
        18,
        incoming_response_ == catalogue::ResponseId::GreetingHello
            ? "M5 / SIDE = ZAMKNIJ"
            : "M5 OK",
        incoming_response_ == catalogue::ResponseId::GreetingHello ? 1 : 2,
        board::DisplayColor::AccentGreen,
        board::DisplayColor::Navy);
}

void CommunicatorApp::render_wait_decision()
{
    clear_screen();
    draw_header(u8"ODPOWIEDŹ");

    board_.draw_polish_ui_text_region(
        10,
        22,
        220,
        16,
        catalogue::response_text(incoming_response_),
        1,
        board::DisplayColor::Ivory,
        board::DisplayColor::Navy);
    board_.draw_polish_ui_text_region(
        10,
        40,
        220,
        14,
        "CO DALEJ?",
        1,
        board::DisplayColor::MutedBlue,
        board::DisplayColor::Navy);

    const char* choices[2] = {"OK", u8"ZACZEKAĆ?"};
    for (std::uint8_t index = 0; index < 2; ++index) {
        const bool selected = index == selected_wait_decision_index_;
        const std::int16_t y =
            static_cast<std::int16_t>(61 + index * 24);

        board_.draw_polish_ui_text_region(
            12,
            y,
            216,
            20,
            choices[index],
            1,
            selected
                ? board::DisplayColor::Ivory
                : board::DisplayColor::MutedBlue,
            selected
                ? board::DisplayColor::PanelNavy
                : board::DisplayColor::Navy);
    }

    board_.draw_polish_ui_text_region(
        8,
        116,
        224,
        14,
        u8"M5 WYBIERZ  |  SIDE DALEJ",
        1,
        board::DisplayColor::MutedBlue,
        board::DisplayColor::Navy);
}

void CommunicatorApp::render_human_ok_received()
{
    clear_screen();
    draw_header("POTWIERDZENIE");

    board_.draw_polish_ui_text_region(
        86,
        46,
        80,
        38,
        "OK",
        3,
        board::DisplayColor::AccentGreen,
        board::DisplayColor::Navy);
    board_.draw_polish_ui_text_region(
        8,
        112,
        224,
        18,
        "M5 / SIDE = ZAMKNIJ",
        1,
        board::DisplayColor::MutedBlue,
        board::DisplayColor::Navy);
}

void CommunicatorApp::render_signal_alert(bool wide_arcs)
{
    clear_screen();

    draw_ringing_arcs(wide_arcs);
    draw_bell_glyph(
        120,
        55,
        3,
        board::DisplayColor::Orange);

    board_.draw_polish_ui_text_region(
        47,
        88,
        146,
        30,
        u8"SYGNAŁ",
        3,
        board::DisplayColor::Orange,
        board::DisplayColor::Navy);

    board_.draw_polish_ui_text_region(
        34,
        120,
        172,
        12,
        "DOWOLNY PRZYCISK = ZAMKNIJ",
        1,
        board::DisplayColor::MutedBlue,
        board::DisplayColor::Navy);
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
        board::DisplayColor::Orange);
    board_.draw_line(
        static_cast<std::int16_t>(120 - offset - 7),
        48,
        static_cast<std::int16_t>(120 - offset),
        lower_y,
        board::DisplayColor::Orange);

    board_.draw_line(
        static_cast<std::int16_t>(120 + offset),
        upper_y,
        static_cast<std::int16_t>(120 + offset + 7),
        48,
        board::DisplayColor::Orange);
    board_.draw_line(
        static_cast<std::int16_t>(120 + offset + 7),
        48,
        static_cast<std::int16_t>(120 + offset),
        lower_y,
        board::DisplayColor::Orange);
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
        board::DisplayColor::Ivory,
        board::DisplayColor::Navy);
}

void CommunicatorApp::draw_header(const char* title)
{
    board_.draw_polish_ui_text_region(
        8,
        5,
        224,
        14,
        title,
        1,
        board::DisplayColor::Ivory,
        board::DisplayColor::Navy);
}

void CommunicatorApp::draw_signal_bars(
    std::uint8_t bars,
    bool reachable)
{
    constexpr std::int16_t base_y = 40;
    constexpr std::int16_t start_x = 192;
    constexpr std::int16_t heights[] = {4, 8, 12};

    for (std::uint8_t index = 0; index < 3; ++index) {
        const bool active_bar = reachable && index < bars;
        const board::DisplayColor color =
            active_bar
                ? board::DisplayColor::AccentGreen
                : board::DisplayColor::MutedBlue;
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
