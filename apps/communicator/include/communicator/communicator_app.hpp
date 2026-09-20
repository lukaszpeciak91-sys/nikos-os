#pragma once

#include <cstdint>

#include "board/board.hpp"
#include "communicator/communicator_catalogue.hpp"
#include "messaging/messaging_service.hpp"

namespace nikos::communicator {

class CommunicatorApp final {
public:
    enum class UpdateResult : std::uint8_t {
        Running,
        ExitRequested,
    };

    CommunicatorApp(
        board::Board& board,
        messaging::Service& messaging,
        const char* peer_label);

    bool begin();
    bool end();
    UpdateResult update();

    // Used by the shell to surface a background message by opening Communicator.
    bool accept_incoming(const messaging::IncomingMessage& message);

private:
    enum class State : std::uint8_t {
        Main,
        WaitingForResponse,
        IncomingPreset,
        ChoosingResponse,
        WaitingForHumanAck,
        IncomingResponse,
        WaitDecision,
        WaitingForWaitResponse,
        HumanOkReceived,
    };

    void poll_expected_incoming();
    void handle_input(const board::InputState& input);

    void handle_main_input(const board::InputState& input);
    void handle_incoming_preset_input(const board::InputState& input);
    void handle_response_choice_input(const board::InputState& input);
    void handle_incoming_response_input(const board::InputState& input);
    void handle_wait_decision_input(const board::InputState& input);
    void handle_human_ok_input(const board::InputState& input);

    bool send_selected_preset();
    bool send_selected_response();
    bool send_human_ok(std::uint32_t reference_message_id);
    bool send_wait_followup();

    bool can_accept_incoming() const;
    void notify_incoming();

    std::uint8_t signal_bars() const;
    void render_current();
    void render_main();
    void render_main_if_status_changed();
    void render_waiting_for_response();
    void render_incoming_preset();
    void render_response_choices();
    void render_waiting_for_human_ack();
    void render_incoming_response();
    void render_wait_decision();
    void render_human_ok_received();

    void clear_screen();
    void draw_header(const char* title);
    void draw_signal_bars(std::uint8_t bars, bool reachable);

    board::Board& board_;
    messaging::Service& messaging_;
    const char* peer_label_;

    bool active_ = false;
    State state_ = State::Main;

    std::uint8_t selected_preset_index_ = 0;
    std::uint8_t selected_response_index_ = 0;
    std::uint8_t selected_wait_decision_index_ = 0;

    catalogue::PresetId sent_preset_ = catalogue::PresetId::Greeting;
    catalogue::PresetId incoming_preset_ = catalogue::PresetId::Greeting;
    catalogue::ResponseId incoming_response_ =
        catalogue::ResponseId::GreetingHello;
    catalogue::ResponseSet response_set_{};

    messaging::IncomingMessage current_incoming_{};
    std::uint32_t expected_response_reference_ = 0;
    std::uint32_t expected_human_ack_reference_ = 0;
    std::uint32_t last_greeting_message_id_ = 0;

    bool rendered_peer_state_valid_ = false;
    bool rendered_peer_reachable_ = false;
    std::uint8_t rendered_signal_bars_ = 0;
};

}  // namespace nikos::communicator
