#include "communicator/communicator_app.hpp"

#include <algorithm>
#include <cstddef>

namespace {

constexpr std::uint32_t kNotificationToneMs = 90;
constexpr float kNotificationToneHz = 2600.0F;

constexpr std::int16_t kScreenWidth = 240;
constexpr std::int16_t kScreenHeight = 135;

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
    render_current();
    return profile_ok;
}

bool CommunicatorApp::end()
{
    active_ = false;

    if (state_ == State::HumanOkReceived) {
        state_ = State::Main;
    }

    return messaging_.set_rx_profile(messaging::RxProfile::Background);
}

CommunicatorApp::UpdateResult CommunicatorApp::update()
{
    messaging::DeliveryReceipt receipt;
    (void)messaging_.poll_delivery(receipt);

    (void)process_incoming();

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
        // SYGNAŁ/RING UX is intentionally outside Communicator v0.1 core UX.
        return false;
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

        if (!idle_message && !wait_followup) {
            return false;
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

    return expected_response_reference_ != 0
        && message.reference_message_id == expected_response_reference_
        && catalogue::response_allowed_for(sent_preset_, response);
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
    if (input.secondary_short) {
        selected_preset_index_ = static_cast<std::uint8_t>(
            (selected_preset_index_ + 1U) % catalogue::kPresetOrder.size());
        render_main();
        return;
    }

    if (input.primary_short && messaging_.peer_reachable()) {
        (void)send_selected_preset();
    }
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
        state_ = State::Main;
        render_main();
    }
}

bool CommunicatorApp::send_selected_preset()
{
    if (!messaging_.peer_reachable()) {
        return false;
    }

    const catalogue::PresetId preset =
        catalogue::kPresetOrder[selected_preset_index_];

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

    const std::size_t selected = selected_preset_index_;
    std::size_t first = selected > 0 ? selected - 1U : 0U;
    const std::size_t max_first = catalogue::kPresetOrder.size() - 3U;
    first = std::min(first, max_first);

    for (std::size_t slot = 0; slot < 3; ++slot) {
        const std::size_t index = first + slot;
        const bool selected_row = index == selected;
        const std::int16_t y =
            static_cast<std::int16_t>(53 + slot * 20);

        const board::DisplayColor foreground =
            reachable && selected_row
                ? board::DisplayColor::Ivory
                : board::DisplayColor::MutedBlue;
        const board::DisplayColor background =
            reachable && selected_row
                ? board::DisplayColor::PanelNavy
                : board::DisplayColor::Navy;

        board_.draw_polish_ui_text_region(
            12,
            y,
            216,
            17,
            catalogue::preset_text(catalogue::kPresetOrder[index]),
            1,
            foreground,
            background);

        if (reachable && selected_row) {
            board_.draw_line(
                8,
                y,
                8,
                static_cast<std::int16_t>(y + 13),
                board::DisplayColor::AccentGreen);
        }
    }

    board_.draw_polish_ui_text_region(
        8,
        116,
        224,
        14,
        reachable
            ? u8"M5 WYŚLIJ  |  SIDE DALEJ"
            : u8"BRAK ŁĄCZNOŚCI  |  SIDE DALEJ",
        1,
        reachable
            ? board::DisplayColor::MutedBlue
            : board::DisplayColor::MutedBlue,
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
