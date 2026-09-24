#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

#include "communicator_protocol/communicator_protocol.hpp"
#include "radio/radio.hpp"

namespace nikos::messaging {

enum class RxProfile : std::uint8_t {
    // Legacy profile names retained for diagnostics/config compatibility:
    // Foreground is now the temporary logical-delivery boost, not UI state.
    Foreground,
    Background,
};

struct RxSchedule {
    std::uint16_t interval_ms = 0;
    std::uint16_t wake_window_ms = 0;
    std::uint32_t reachability_timeout_ms = 0;
};

struct Config {
    std::uint8_t channel = 6;
    radio::Mode mode = radio::Mode::Normal;
    std::uint32_t presence_interval_ms = 2000;
    std::uint16_t presence_jitter_ms = 250;
    std::uint32_t retry_interval_ms = 1000;
    std::uint16_t retry_jitter_ms = 250;
    std::uint8_t max_send_attempts = 8;
    std::uint32_t delivery_timeout_ms = 12000;
    std::uint16_t tx_result_timeout_ms = 500;
    std::uint16_t mac_success_ack_grace_margin_ms = 250;
    RxSchedule foreground_rx{1000, 500, 7000};
    RxSchedule background_rx{3000, 500, 20000};
};

enum class IncomingKind : std::uint8_t {
    PresetMessage,
    PresetResponse,
    Ring,
};

struct IncomingMessage {
    IncomingKind kind = IncomingKind::PresetMessage;
    std::uint32_t logical_message_id = 0;
    std::uint16_t preset_id = 0;
    std::uint16_t response_id = 0;
};

enum class DeliveryKind : std::uint8_t {
    PresetMessage,
    PresetResponse,
    Ring,
};

enum class DeliveryOutcome : std::uint8_t {
    Delivered,
    Failed,
};

struct DeliveryReceipt {
    DeliveryKind kind = DeliveryKind::PresetMessage;
    DeliveryOutcome outcome = DeliveryOutcome::Delivered;
    std::uint32_t logical_message_id = 0;
    std::uint32_t send_attempt_count = 0;
    std::uint32_t send_request_failure_count = 0;
    std::uint32_t latency_ms = 0;
};

class Service final {
public:
    explicit Service(radio::RadioService& radio);

    bool begin(const Config& config);
    bool stop();
    void update();

    bool pause_transport();
    bool resume_transport();
    bool transport_active() const;

    RxProfile rx_profile() const;
    RxSchedule current_rx_schedule() const;

    bool set_radio_mode(radio::Mode mode);
    radio::Mode radio_mode() const;

    bool send_preset_message(std::uint16_t message_id);
    bool send_preset_response(
        std::uint16_t preset_id,
        std::uint16_t response_id);
    bool send_ring();

    bool outgoing_pending() const;
    std::uint32_t outgoing_logical_message_id() const;

    bool peer_known() const;
    // Recent-RX status only; it is not a permission gate for bounded sends.
    bool peer_reachable() const;
    const radio::MacAddress& self_mac() const;
    const radio::MacAddress& peer_mac() const;
    bool latest_peer_rssi(std::int8_t& rssi) const;

    bool peek_incoming(IncomingMessage& message) const;
    bool consume_incoming(std::uint32_t logical_message_id);
    bool poll_delivery(DeliveryReceipt& receipt);

private:
    static constexpr std::size_t kDedupeDepth = 8;
    static constexpr std::size_t kIncomingQueueDepth = 4;
    static constexpr std::size_t kPendingAckDepth = 4;

    enum class UnicastKind : std::uint8_t {
        OutgoingPayload,
        ApplicationAck,
        PresenceReply,
    };

    struct UnicastInFlight {
        bool active = false;
        UnicastKind kind = UnicastKind::OutgoingPayload;
        radio::MacAddress destination{};
        std::uint32_t submitted_ms = 0;
        std::uint32_t logical_message_id = 0;
    };

    struct OutgoingState {
        bool active = false;
        bool completion_pending_transport = false;
        bool superseded = false;
        communicator_protocol::Message message{};
        DeliveryOutcome completed_outcome = DeliveryOutcome::Failed;
        std::uint32_t completed_ms = 0;
        std::uint32_t started_ms = 0;
        std::uint32_t last_send_ms = 0;
        std::uint32_t retry_delay_ms = 0;
        std::uint32_t paused_since_ms = 0;
        std::uint32_t attempts = 0;
        std::uint32_t accepted_submissions = 0;
        std::uint32_t send_request_failures = 0;
        std::uint32_t mac_successes = 0;
        std::uint32_t mac_failures = 0;
        std::uint32_t tx_result_timeouts = 0;
    };

    struct PendingOutgoing {
        bool valid = false;
        communicator_protocol::Message message{};
    };

    struct TrafficCounters {
        std::uint32_t logical_payload_tx_submissions = 0;
        std::uint32_t ack_tx_submissions = 0;
        std::uint32_t ack_send_request_failures = 0;
        std::uint32_t discovery_presence_tx_submissions = 0;
        std::uint32_t presence_reply_tx_submissions = 0;
        std::uint32_t presence_reply_send_request_failures = 0;
        std::uint32_t presence_reply_mac_successes = 0;
        std::uint32_t presence_reply_mac_failures = 0;
        std::uint32_t presence_reply_tx_result_timeouts = 0;
        std::uint32_t logical_mac_successes = 0;
        std::uint32_t logical_mac_failures = 0;
        std::uint32_t logical_tx_result_timeouts = 0;
        std::uint32_t ack_mac_successes = 0;
        std::uint32_t ack_mac_failures = 0;
        std::uint32_t ack_tx_result_timeouts = 0;
        std::uint32_t ack_queue_overflows = 0;
        std::uint32_t delivered_logical_messages = 0;
        std::uint32_t failed_logical_messages = 0;
    };

    bool start_transport();
    RxProfile desired_rx_profile() const;
    bool apply_rx_profile(RxProfile profile);
    bool apply_desired_rx_profile();
    radio::RxPowerConfig rx_power_for(RxProfile profile) const;
    const RxSchedule& rx_schedule_for(RxProfile profile) const;

    void process_radio_events();
    void process_rx(const radio::RxEvent& event);
    void process_tx_result(const radio::TxEvent& event);
    void record_peer_rx(const radio::RxEvent& event);

    void send_discovery_presence(std::uint32_t now_ms);
    bool submit_presence_reply(std::uint32_t now_ms);
    void send_ack(std::uint32_t reference_message_id);
    bool enqueue_ack(std::uint32_t reference_message_id);
    bool submit_next_ack(std::uint32_t now_ms);
    bool start_outgoing(
        communicator_protocol::MessageType type,
        std::uint16_t preset_id,
        std::uint16_t response_id);
    void supersede_outgoing(std::uint32_t now_ms);
    bool activate_pending_outgoing(std::uint32_t now_ms);
    void send_outgoing(std::uint32_t now_ms);
    void service_unicast(std::uint32_t now_ms);
    void handle_missing_tx_result(
        std::uint32_t now_ms,
        bool restart_transport);
    void resolve_in_flight(
        bool success,
        std::uint32_t resolved_ms);
    std::uint32_t next_retry_delay_ms() const;
    std::uint32_t mac_success_ack_grace_ms() const;
    void finish_outgoing(
        DeliveryOutcome outcome,
        std::uint32_t completed_ms);
    void finalize_outgoing_metrics();

    bool is_duplicate(std::uint32_t logical_message_id) const;
    void remember_received(std::uint32_t logical_message_id);
    bool enqueue_incoming(const IncomingMessage& message);

    std::uint32_t next_logical_message_id();
    std::uint32_t now_ms() const;

    radio::RadioService& radio_;
    Config config_{};
    bool started_ = false;
    bool transport_active_ = false;
    bool faulted_ = false;
    RxProfile rx_profile_ = RxProfile::Background;

    bool peer_known_ = false;
    radio::MacAddress peer_mac_{};
    std::uint32_t last_peer_rx_ms_ = 0;
    std::int8_t latest_peer_rssi_ = 0;
    bool latest_peer_rssi_valid_ = false;
    std::uint32_t last_presence_tx_ms_ = 0;
    std::uint32_t current_presence_delay_ms_ = 0;
    bool pending_presence_reply_ = false;

    std::uint32_t next_message_id_ = 1;
    OutgoingState outgoing_{};
    PendingOutgoing pending_outgoing_{};
    UnicastInFlight unicast_in_flight_{};
    TrafficCounters traffic_{};

    std::array<std::uint32_t, kPendingAckDepth> pending_ack_references_{};
    std::size_t pending_ack_head_ = 0;
    std::size_t pending_ack_count_ = 0;

    std::array<std::uint32_t, kDedupeDepth> recent_received_ids_{};
    std::size_t recent_received_count_ = 0;
    std::size_t recent_received_next_ = 0;

    std::array<IncomingMessage, kIncomingQueueDepth> incoming_queue_{};
    std::size_t incoming_head_ = 0;
    std::size_t incoming_count_ = 0;

    DeliveryReceipt delivery_receipt_{};
    bool delivery_ready_ = false;
};

}  // namespace nikos::messaging
