#pragma once

#include <cstdint>

#include "board/board.hpp"
#include "communicator/communicator_catalogue.hpp"
#include "messaging/messaging_service.hpp"
#include "power/display_lifecycle.hpp"
#include "signal_sound/signal_sound_player.hpp"

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
        power::DisplayLifecycle& display_lifecycle,
        signal_sound::Player& signal_sound,
        const char* peer_label);

    bool begin();
    bool end();
    void redraw();
    void reset_session();
    UpdateResult update(const board::InputState& input);

    // Drain retained transport events. User-visible messages use latest-wins;
    // RING remains a separate attention event.
    bool process_incoming();
    bool timer_preemption_active() const;

private:
    enum class State : std::uint8_t {
        Main,
        IncomingPreset,
        ChoosingResponse,
        IncomingResponse,
        WaitDecision,
    };

    enum class DeliveryStatus : std::uint8_t {
        None,
        Sending,
        Delivered,
        Failed,
    };

    bool accept_incoming(const messaging::IncomingMessage& message);
    bool valid_user_message(const messaging::IncomingMessage& message) const;
    void handle_delivery_receipt(
        const messaging::DeliveryReceipt& receipt);
    void handle_input(const board::InputState& input);

    void handle_main_input(const board::InputState& input);
    void handle_options_input(const board::InputState& input);
    void handle_incoming_preset_input(const board::InputState& input);
    void handle_response_choice_input(const board::InputState& input);
    void handle_incoming_response_input(const board::InputState& input);
    void handle_wait_decision_input(const board::InputState& input);

    bool send_selected_preset();
    bool send_signal();
    bool send_selected_response();
    bool send_wait_followup();
    void track_latest_send();

    void notify_incoming();

    void start_signal_alert();
    void update_signal_alert(std::uint32_t now_ms);
    void dismiss_signal_alert();
    bool any_user_button(const board::InputState& input) const;

    std::uint8_t signal_bars() const;
    void render_current();
    void render_main();
    void render_options();
    void render_main_if_status_changed();
    void render_incoming_preset();
    void render_response_choices();
    void render_incoming_response();
    void render_wait_decision();
    void render_signal_unavailable();
    void render_signal_alert(bool wide_arcs);
    void draw_bell_glyph(
        std::int16_t center_x,
        std::int16_t center_y,
        std::int16_t scale,
        board::DisplayColor color);
    void draw_ringing_arcs(bool wide_arcs);

    void clear_screen();
    void draw_header(const char* title);
    void draw_signal_bars(std::uint8_t bars, bool reachable);

    board::Board& board_;
    messaging::Service& messaging_;
    power::DisplayLifecycle& display_lifecycle_;
    signal_sound::Player& signal_sound_;
    const char* peer_label_;

    bool active_ = false;
    bool foreground_exit_requested_ = false;
    bool options_active_ = false;
    bool radio_mode_change_failed_ = false;
    State state_ = State::Main;

    std::uint8_t selected_main_index_ = 0;
    std::uint8_t selected_options_index_ = 0;
    std::uint8_t selected_response_index_ = 0;

    catalogue::PresetId incoming_preset_ = catalogue::PresetId::Greeting;
    catalogue::ResponseId incoming_response_ =
        catalogue::ResponseId::GreetingHello;
    catalogue::ResponseSet response_set_{};
    messaging::IncomingMessage current_incoming_{};

    std::uint32_t latest_outgoing_message_id_ = 0;
    DeliveryStatus latest_delivery_status_ = DeliveryStatus::None;

    bool signal_unavailable_feedback_ = false;
    bool signal_alert_active_ = false;
    bool signal_return_to_launcher_ = false;
    bool signal_audio_complete_rendered_ = false;
    bool signal_animation_wide_ = false;
    std::uint32_t signal_last_animation_ms_ = 0;

    bool rendered_peer_state_valid_ = false;
    bool rendered_peer_known_ = false;
    bool rendered_peer_reachable_ = false;
    std::uint8_t rendered_signal_bars_ = 0;
};

}  // namespace nikos::communicator
