#include "communicator/communicator_app.hpp"

#include <cstddef>

#include "esp_timer.h"

namespace {

constexpr std::uint32_t kNotificationToneMs = 90;
constexpr float kNotificationToneHz = 2600.0F;

constexpr std::uint32_t kSignalAnimationMs = 120;

constexpr std::int16_t kScreenWidth = 240;
constexpr std::int16_t kScreenHeight = 135;

std::uint8_t preset_text_scale(
    nikos::communicator::catalogue::PresetId preset)
{
    using nikos::communicator::catalogue::PresetId;
    switch (preset) {
        case PresetId::Greeting:
        case PresetId::Cans:
        case PresetId::Wait:
            return 3;
        case PresetId::CanTalk:
        case PresetId::Walk:
        default:
            return 2;
    }
}

std::uint8_t response_text_scale(
    nikos::communicator::catalogue::ResponseId response)
{
    using nikos::communicator::catalogue::ResponseId;
    switch (response) {
        case ResponseId::GreetingHello:
        case ResponseId::Soon:
        case ResponseId::Later:
        case ResponseId::Have:
        case ResponseId::DontHave:
        case ResponseId::WillCheck:
        case ResponseId::DontWait:
            return 3;
        case ResponseId::YesComing:
        case ResponseId::Busy:
        case ResponseId::NotToday:
        case ResponseId::YesWait:
        default:
            return 2;
    }
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
    signal_sound::Player& signal_sound,
    const char* peer_label)
    : board_(board),
      messaging_(messaging),
      signal_sound_(signal_sound),
      peer_label_(peer_label)
{
}

bool CommunicatorApp::begin()
{
    active_ = true;
    foreground_exit_requested_ = false;
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
        signal_sound_.stop();
        signal_alert_active_ = false;
        signal_return_to_launcher_ = false;
        signal_audio_complete_rendered_ = false;
    }

    active_ = false;
    return messaging_.set_rx_profile(messaging::RxProfile::Background);
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

    sent_preset_ = catalogue::PresetId::Greeting;
    incoming_preset_ = catalogue::PresetId::Greeting;
    incoming_response_ = catalogue::ResponseId::GreetingHello;
    response_set_ = catalogue::ResponseSet{};

    current_incoming_ = messaging::IncomingMessage{};
    deferred_incoming_ = messaging::IncomingMessage{};
    deferred_incoming_valid_ = false;
    suspended_waiting_ = SuspendedWaitingContext{};

    expected_response_reference_ = 0;
    pending_response_delivery_id_ = 0;
    last_greeting_message_id_ = 0;
    delivery_failure_restore_suspended_ = false;

    signal_alert_active_ = false;
    signal_return_to_launcher_ = false;
    signal_audio_complete_rendered_ = false;
    signal_animation_wide_ = false;
    signal_last_animation_ms_ = 0;

    rendered_peer_state_valid_ = false;
    rendered_peer_known_ = false;
    rendered_peer_reachable_ = false;
    rendered_signal_bars_ = 0;
}

CommunicatorApp::UpdateResult CommunicatorApp::update()
{
    messaging::DeliveryReceipt receipt;
    if (messaging_.poll_delivery(receipt)) {
        handle_delivery_receipt(receipt);
    }

    if (!signal_alert_active_ && state_ != State::DeliveryFailed) {
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

    if (state_ == State::Main && !options_active_) {
        render_main_if_status_changed();
    }

    const board::InputState input = board_.poll_input();

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
            state_ == State::WaitingForResponseDelivery
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
    suspended_waiting_ = SuspendedWaitingContext{};
    state_ = State::WaitingForResponse;

    if (render) {
        render_waiting_for_response();
    }
}

void CommunicatorApp::handle_delivery_receipt(
    const messaging::DeliveryReceipt& receipt)
{
    if (receipt.outcome == messaging::DeliveryOutcome::Delivered) {
        if (receipt.logical_message_id == pending_response_delivery_id_) {
            pending_response_delivery_id_ = 0;

            // A peer follow-up may already have become the active human
            // exchange. Do not clobber it merely because the response's
            // technical ACK arrived meanwhile.
            if (state_ == State::WaitingForResponseDelivery) {
                if (suspended_waiting_.valid) {
                    restore_suspended_waiting(
                        active_ && !signal_alert_active_);
                } else {
                    state_ = State::Main;
                    if (active_ && !signal_alert_active_) {
                        render_main();
                    }
                }
            }
        }
        return;
    }

    const bool failed_suspended_delivery =
        suspended_waiting_.valid
        && receipt.logical_message_id
            == suspended_waiting_.expected_response_reference;
    if (failed_suspended_delivery) {
        // The peer exchange that caused this outgoing request to be
        // suspended is already the active conversation. Drop only the
        // failed suspended context and leave that accepted exchange intact.
        suspended_waiting_ = SuspendedWaitingContext{};
        delivery_failure_restore_suspended_ = false;
        return;
    }

    delivery_failure_restore_suspended_ =
        suspended_waiting_.valid;

    if (receipt.logical_message_id == expected_response_reference_) {
        expected_response_reference_ = 0;
    }
    if (receipt.logical_message_id == pending_response_delivery_id_) {
        pending_response_delivery_id_ = 0;
    }
    if (receipt.logical_message_id == last_greeting_message_id_) {
        last_greeting_message_id_ = 0;
    }

    options_active_ = false;
    radio_mode_change_failed_ = false;
    state_ = State::DeliveryFailed;

    if (active_ && !signal_alert_active_) {
        render_delivery_failed();
    }
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
        case State::DeliveryFailed:
            handle_delivery_failed_input(input);
            break;
        case State::WaitingForResponse:
        case State::WaitingForResponseDelivery:
        case State::WaitingForWaitResponse:
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

    if (input.secondary_short) {
        std::uint8_t next = static_cast<std::uint8_t>(
            (selected_main_index_ + 1U) % choice_count);

        // SYGNAŁ is unavailable only until a peer identity is known.
        if (!peer_known && next == signal_index) {
            next = options_index;
        }

        selected_main_index_ = next;
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

    if (!peer_known) {
        return;
    }

    if (selected_main_index_ == signal_index) {
        (void)send_signal();
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

void CommunicatorApp::handle_delivery_failed_input(
    const board::InputState& input)
{
    if (!input.primary_short && !input.secondary_short) {
        return;
    }

    const bool restore_suspended =
        delivery_failure_restore_suspended_
        && suspended_waiting_.valid;
    delivery_failure_restore_suspended_ = false;

    if (restore_suspended) {
        restore_suspended_waiting(true);
    } else {
        state_ = State::Main;
        render_main();
    }
}

bool CommunicatorApp::send_selected_preset()
{
    if (!messaging_.peer_known()) {
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
    if (!messaging_.peer_known()) {
        return false;
    }

    // SYGNAŁ is intentionally outside the preset catalogue and conversation
    // state machine. Delivery still uses messaging RING retry/ACK/dedupe.
    return messaging_.send_ring();
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
            static_cast<std::uint16_t>(response),
            current_incoming_.logical_message_id)) {
        return false;
    }

    if (incoming_preset_ == catalogue::PresetId::Greeting) {
        state_ = State::Main;
        render_main();
        return true;
    }

    pending_response_delivery_id_ =
        messaging_.outgoing_logical_message_id();
    state_ = State::WaitingForResponseDelivery;
    render_waiting_for_response_delivery();
    return true;
}

bool CommunicatorApp::send_wait_followup()
{
    if (!messaging_.peer_known()
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
    signal_sound_.stop();
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
    signal_audio_complete_rendered_ = false;
    signal_animation_wide_ = false;

    const std::uint32_t now = now_ms();
    signal_last_animation_ms_ = now;

    board_.wake_display();
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
        case State::WaitingForResponseDelivery:
            render_waiting_for_response_delivery();
            break;
        case State::IncomingResponse:
            render_incoming_response();
            break;
        case State::WaitDecision:
            render_wait_decision();
            break;
        case State::DeliveryFailed:
            render_delivery_failed();
            break;
    }
}

void CommunicatorApp::render_main()
{
    clear_screen();
    draw_header("KOMUNIKATOR");

    const bool peer_known = messaging_.peer_known();
    const bool recently_seen = messaging_.peer_reachable();
    const std::uint8_t bars = signal_bars();
    const std::size_t signal_index = catalogue::kPresetOrder.size();
    const std::size_t options_index = signal_index + 1U;
    const std::size_t return_index = options_index + 1U;

    if (!peer_known && selected_main_index_ == signal_index) {
        selected_main_index_ =
            static_cast<std::uint8_t>(options_index);
    }

    board_.draw_polish_ui_text_region(
        10,
        22,
        82,
        11,
        peer_label_,
        1,
        board::DisplayColor::SecondaryText,
        board::DisplayColor::Background);

    board_.draw_polish_ui_text_region(
        94,
        22,
        96,
        11,
        !peer_known
            ? "SZUKAM..."
            : (recently_seen ? "DOSTĘPNY" : "GOTOWY"),
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

    board_.draw_polish_ui_text_region(
        8,
        38,
        224,
        48,
        "",
        1,
        board::DisplayColor::PrimaryText,
        board::DisplayColor::Surface);

    board_.draw_polish_ui_text_region(
        15,
        49,
        210,
        30,
        catalogue::preset_text(displayed_preset),
        preset_text_scale(displayed_preset),
        preset_focused && peer_known
            ? board::DisplayColor::PrimaryText
            : board::DisplayColor::SecondaryText,
        board::DisplayColor::Surface);

    if (preset_focused) {
        const board::DisplayColor focus_color =
            peer_known
                ? board::DisplayColor::Accent
                : board::DisplayColor::SecondaryText;
        board_.draw_line(8, 38, 231, 38, focus_color);
        board_.draw_line(8, 85, 231, 85, focus_color);
        board_.draw_line(8, 38, 8, 85, focus_color);
        board_.draw_line(231, 38, 231, 85, focus_color);
    }

    constexpr const char* kPresetCounters[] = {
        "1 / 5", "2 / 5", "3 / 5", "4 / 5", "5 / 5",
    };
    board_.draw_polish_ui_text_region(
        103,
        88,
        40,
        11,
        kPresetCounters[displayed_preset_index],
        1,
        board::DisplayColor::SecondaryText,
        board::DisplayColor::Background);

    const bool signal_selected =
        peer_known && selected_main_index_ == signal_index;
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

    board_.draw_polish_ui_text_region(
        8, 100, 76, 20, "", 1, signal_color, signal_background);
    board_.draw_polish_ui_text_region(
        10, 101, 72, 18, "SYGNAŁ", 2, signal_color, signal_background);
    if (signal_selected) {
        board_.draw_line(8, 100, 8, 118, board::DisplayColor::Attention);
    }

    const board::DisplayColor options_background =
        options_selected
            ? board::DisplayColor::Surface
            : board::DisplayColor::Background;
    board_.draw_polish_ui_text_region(
        84,
        100,
        72,
        20,
        "",
        1,
        board::DisplayColor::PrimaryText,
        options_background);
    board_.draw_polish_ui_text_region(
        89,
        101,
        65,
        18,
        "OPCJE",
        2,
        options_selected
            ? board::DisplayColor::PrimaryText
            : board::DisplayColor::SecondaryText,
        options_background);
    if (options_selected) {
        board_.draw_line(84, 100, 84, 118, board::DisplayColor::Accent);
    }

    const board::DisplayColor return_background =
        return_selected
            ? board::DisplayColor::Surface
            : board::DisplayColor::Background;
    board_.draw_polish_ui_text_region(
        156,
        100,
        76,
        20,
        "",
        1,
        board::DisplayColor::PrimaryText,
        return_background);
    board_.draw_polish_ui_text_region(
        159,
        101,
        71,
        18,
        "POWRÓT",
        2,
        return_selected
            ? board::DisplayColor::PrimaryText
            : board::DisplayColor::SecondaryText,
        return_background);
    if (return_selected) {
        board_.draw_line(156, 100, 156, 118, board::DisplayColor::Accent);
    }

    board_.draw_polish_ui_text_region(
        31,
        123,
        190,
        10,
        "M5 WYBIERZ   SIDE DALEJ",
        1,
        board::DisplayColor::SecondaryText,
        board::DisplayColor::Background);

    rendered_peer_state_valid_ = true;
    rendered_peer_known_ = peer_known;
    rendered_peer_reachable_ = recently_seen;
    rendered_signal_bars_ = bars;
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

    board_.draw_polish_ui_text_region(
        8,
        29,
        224,
        44,
        "",
        1,
        board::DisplayColor::PrimaryText,
        mode_background);

    board_.draw_polish_ui_text_region(
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

    board_.draw_polish_ui_text_region(
        16,
        51,
        208,
        21,
        mode_text,
        2,
        board::DisplayColor::PrimaryText,
        mode_background);

    if (mode_selected) {
        board_.draw_line(
            8,
            30,
            8,
            69,
            board::DisplayColor::Accent);
    }

    board_.draw_polish_ui_text_region(
        12,
        78,
        216,
        22,
        "POWRÓT",
        2,
        return_selected
            ? board::DisplayColor::PrimaryText
            : board::DisplayColor::SecondaryText,
        return_selected
            ? board::DisplayColor::Surface
            : board::DisplayColor::Background);

    if (return_selected) {
        board_.draw_line(
            8,
            79,
            8,
            96,
            board::DisplayColor::Accent);
    }

    board_.draw_polish_ui_text_region(
        18,
        102,
        204,
        13,
        radio_mode_change_failed_
            ? "NIE UDAŁO SIĘ"
            : "USTAW TAK SAMO NA OBU",
        1,
        radio_mode_change_failed_
            ? board::DisplayColor::Danger
            : board::DisplayColor::SecondaryText,
        board::DisplayColor::Background);

    board_.draw_polish_ui_text_region(
        8,
        120,
        224,
        13,
        "M5 WYBIERZ  |  SIDE DALEJ",
        1,
        board::DisplayColor::SecondaryText,
        board::DisplayColor::Background);
}

void CommunicatorApp::render_main_if_status_changed()
{
    const bool peer_known = messaging_.peer_known();
    const bool recently_seen = messaging_.peer_reachable();
    const std::uint8_t bars = signal_bars();

    if (!rendered_peer_state_valid_
        || peer_known != rendered_peer_known_
        || recently_seen != rendered_peer_reachable_
        || bars != rendered_signal_bars_) {
        render_main();
    }
}

void CommunicatorApp::render_waiting_for_response()
{
    clear_screen();
    draw_header("KOMUNIKATOR");

    board_.draw_polish_ui_text_region(
        8,
        32,
        224,
        56,
        "",
        1,
        board::DisplayColor::PrimaryText,
        board::DisplayColor::Surface);
    board_.draw_polish_ui_text_region(
        15,
        45,
        210,
        32,
        catalogue::preset_text(sent_preset_),
        preset_text_scale(sent_preset_),
        board::DisplayColor::PrimaryText,
        board::DisplayColor::Surface);

    board_.draw_polish_ui_text_region(
        45,
        96,
        170,
        15,
        "CZEKAM NA ODPOWIEDŹ...",
        1,
        board::DisplayColor::SecondaryText,
        board::DisplayColor::Background);
    board_.draw_polish_ui_text_region(
        58,
        121,
        150,
        11,
        "SIDE HOLD = MENU",
        1,
        board::DisplayColor::SecondaryText,
        board::DisplayColor::Background);
}

void CommunicatorApp::render_incoming_preset()
{
    clear_screen();
    draw_header("WIADOMOŚĆ");

    board_.draw_polish_ui_text_region(
        8,
        31,
        224,
        60,
        "",
        1,
        board::DisplayColor::PrimaryText,
        board::DisplayColor::Surface);
    board_.draw_polish_ui_text_region(
        15,
        45,
        210,
        34,
        catalogue::preset_text(incoming_preset_),
        preset_text_scale(incoming_preset_),
        board::DisplayColor::PrimaryText,
        board::DisplayColor::Surface);
    board_.draw_line(
        8,
        31,
        8,
        90,
        board::DisplayColor::Accent);

    const bool can_dismiss =
        incoming_preset_ == catalogue::PresetId::Greeting;

    board_.draw_polish_ui_text_region(
        27,
        108,
        205,
        20,
        can_dismiss
            ? "M5 ODPOWIEDŹ   SIDE ZAMKNIJ"
            : "M5 ODPOWIEDŹ",
        1,
        board::DisplayColor::SecondaryText,
        board::DisplayColor::Background);
}

void CommunicatorApp::render_response_choices()
{
    clear_screen();
    draw_header("ODPOWIEDŹ");

    board_.draw_polish_ui_text_region(
        8,
        23,
        224,
        38,
        "",
        1,
        board::DisplayColor::PrimaryText,
        board::DisplayColor::Surface);
    board_.draw_polish_ui_text_region(
        14,
        30,
        212,
        25,
        catalogue::preset_text(incoming_preset_),
        2,
        board::DisplayColor::SecondaryText,
        board::DisplayColor::Surface);

    const catalogue::ResponseId response =
        response_set_.ids[selected_response_index_];
    board_.draw_polish_ui_text_region(
        8,
        66,
        224,
        39,
        "",
        1,
        board::DisplayColor::PrimaryText,
        board::DisplayColor::Surface);
    board_.draw_polish_ui_text_region(
        14,
        74,
        212,
        27,
        catalogue::response_text(response),
        response_text_scale(response),
        board::DisplayColor::PrimaryText,
        board::DisplayColor::Surface);
    board_.draw_line(
        8,
        66,
        8,
        104,
        board::DisplayColor::Accent);

    board_.draw_polish_ui_text_region(
        102,
        108,
        46,
        10,
        choice_counter(selected_response_index_, response_set_.count),
        1,
        board::DisplayColor::SecondaryText,
        board::DisplayColor::Background);

    board_.draw_polish_ui_text_region(
        31,
        122,
        190,
        11,
        "M5 WYBIERZ   SIDE DALEJ",
        1,
        board::DisplayColor::SecondaryText,
        board::DisplayColor::Background);
}

void CommunicatorApp::render_waiting_for_response_delivery()
{
    clear_screen();
    draw_header("ODPOWIEDŹ");

    board_.draw_polish_ui_text_region(
        10,
        27,
        220,
        14,
        catalogue::preset_text(incoming_preset_),
        1,
        board::DisplayColor::SecondaryText,
        board::DisplayColor::Background);

    const catalogue::ResponseId response =
        response_set_.ids[selected_response_index_];
    board_.draw_polish_ui_text_region(
        8,
        48,
        224,
        43,
        "",
        1,
        board::DisplayColor::PrimaryText,
        board::DisplayColor::Surface);
    board_.draw_polish_ui_text_region(
        14,
        57,
        212,
        28,
        catalogue::response_text(response),
        response_text_scale(response),
        board::DisplayColor::PrimaryText,
        board::DisplayColor::Surface);

    board_.draw_polish_ui_text_region(
        70,
        101,
        130,
        13,
        "DOSTARCZAM...",
        1,
        board::DisplayColor::StatusActive,
        board::DisplayColor::Background);
}

void CommunicatorApp::render_incoming_response()
{
    clear_screen();
    draw_header("ODPOWIEDŹ");

    board_.draw_polish_ui_text_region(
        8,
        36,
        224,
        58,
        "",
        1,
        board::DisplayColor::PrimaryText,
        board::DisplayColor::Surface);
    board_.draw_polish_ui_text_region(
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
        36,
        8,
        93,
        board::DisplayColor::Accent);

    board_.draw_polish_ui_text_region(
        48,
        112,
        180,
        18,
        "M5 / SIDE = ZAMKNIJ",
        1,
        board::DisplayColor::SecondaryText,
        board::DisplayColor::Background);
}

void CommunicatorApp::render_wait_decision()
{
    clear_screen();
    draw_header("ODPOWIEDŹ");

    board_.draw_polish_ui_text_region(
        8,
        31,
        224,
        46,
        "",
        1,
        board::DisplayColor::PrimaryText,
        board::DisplayColor::Surface);
    board_.draw_polish_ui_text_region(
        14,
        41,
        212,
        30,
        catalogue::response_text(incoming_response_),
        response_text_scale(incoming_response_),
        board::DisplayColor::PrimaryText,
        board::DisplayColor::Surface);

    board_.draw_polish_ui_text_region(
        28,
        87,
        195,
        18,
        "M5    ZACZEKAĆ?",
        2,
        board::DisplayColor::Accent,
        board::DisplayColor::Background);
    board_.draw_polish_ui_text_region(
        28,
        110,
        195,
        17,
        "SIDE  ZAMKNIJ",
        1,
        board::DisplayColor::SecondaryText,
        board::DisplayColor::Background);
}

void CommunicatorApp::render_delivery_failed()
{
    clear_screen();
    draw_header("KOMUNIKATOR");

    board_.draw_polish_ui_text_region(
        12,
        48,
        216,
        24,
        "NIE DOSTARCZONO",
        2,
        board::DisplayColor::Danger,
        board::DisplayColor::Background);
    board_.draw_polish_ui_text_region(
        8,
        112,
        224,
        18,
        "M5 / SIDE = POWRÓT",
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

    board_.draw_polish_ui_text_region(
        47,
        88,
        146,
        30,
        "SYGNAŁ",
        3,
        board::DisplayColor::Attention,
        board::DisplayColor::Background);

    board_.draw_polish_ui_text_region(
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
    board_.draw_polish_ui_text_region(
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
