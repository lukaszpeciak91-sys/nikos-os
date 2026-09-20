#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

#include "communicator_protocol/communicator_protocol.hpp"
#include "radio/radio.hpp"

namespace nikos::messaging {

enum class RxProfile : std::uint8_t {
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
    std::uint32_t retry_interval_ms = 500;
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
    std::uint32_t reference_message_id = 0;
    std::uint16_t value_id = 0;
};

enum class DeliveryKind : std::uint8_t {
    PresetMessage,
    PresetResponse,
    Ring,
};

struct DeliveryReceipt {
    DeliveryKind kind = DeliveryKind::PresetMessage;
    std::uint32_t logical_message_id = 0;
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

    bool set_rx_profile(RxProfile profile);
    RxProfile rx_profile() const;

    bool send_preset_message(std::uint16_t message_id);
    bool send_preset_response(
        std::uint16_t response_id,
        std::uint32_t reference_message_id);
    bool send_ring();

    bool outgoing_pending() const;
    std::uint32_t outgoing_logical_message_id() const;

    bool peer_known() const;
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

    struct OutgoingState {
        bool active = false;
        communicator_protocol::Message message{};
        std::uint32_t last_send_ms = 0;
        std::uint32_t attempts = 0;
    };

    bool start_transport();
    radio::RxPowerConfig rx_power_for(RxProfile profile) const;

    void process_radio_events();
    void process_rx(const radio::RxEvent& event);
    void record_peer_rx(const radio::RxEvent& event);

    void send_presence(std::uint32_t now_ms);
    void send_ack(std::uint32_t reference_message_id);
    bool start_outgoing(
        communicator_protocol::MessageType type,
        std::uint16_t value_id,
        std::uint32_t reference_message_id);
    void send_outgoing(std::uint32_t now_ms);

    bool is_duplicate(std::uint32_t logical_message_id) const;
    void remember_received(std::uint32_t logical_message_id);
    bool enqueue_incoming(const IncomingMessage& message);
    void publish_delivery();

    std::uint32_t next_logical_message_id();
    std::uint32_t now_ms() const;

    radio::RadioService& radio_;
    Config config_{};
    bool started_ = false;
    bool transport_active_ = false;
    RxProfile rx_profile_ = RxProfile::Background;

    bool peer_known_ = false;
    radio::MacAddress peer_mac_{};
    std::uint32_t last_peer_rx_ms_ = 0;
    std::int8_t latest_peer_rssi_ = 0;
    bool latest_peer_rssi_valid_ = false;
    std::uint32_t last_presence_tx_ms_ = 0;
    std::uint32_t current_presence_delay_ms_ = 0;

    std::uint32_t next_message_id_ = 1;
    OutgoingState outgoing_{};

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
