#include "messaging/messaging_service.hpp"

#include <array>

#include "esp_log.h"
#include "esp_random.h"
#include "esp_timer.h"

namespace {

constexpr char kTag[] = "messaging";

const char* delivery_kind_name(nikos::messaging::DeliveryKind kind)
{
    using nikos::messaging::DeliveryKind;
    switch (kind) {
        case DeliveryKind::PresetResponse:
            return "preset_response";
        case DeliveryKind::Ring:
            return "ring";
        case DeliveryKind::PresetMessage:
        default:
            return "preset_message";
    }
}

const char* delivery_outcome_name(nikos::messaging::DeliveryOutcome outcome)
{
    return outcome == nikos::messaging::DeliveryOutcome::Delivered
        ? "delivered"
        : "failed";
}

bool is_notifiable(nikos::communicator_protocol::MessageType type)
{
    using nikos::communicator_protocol::MessageType;
    return type == MessageType::PresetMessage
        || type == MessageType::PresetResponse
        || type == MessageType::Ring;
}

nikos::messaging::IncomingKind to_incoming_kind(
    nikos::communicator_protocol::MessageType type)
{
    using nikos::communicator_protocol::MessageType;
    switch (type) {
        case MessageType::PresetResponse:
            return nikos::messaging::IncomingKind::PresetResponse;
        case MessageType::Ring:
            return nikos::messaging::IncomingKind::Ring;
        case MessageType::PresetMessage:
        default:
            return nikos::messaging::IncomingKind::PresetMessage;
    }
}

nikos::messaging::DeliveryKind to_delivery_kind(
    nikos::communicator_protocol::MessageType type)
{
    using nikos::communicator_protocol::MessageType;
    switch (type) {
        case MessageType::PresetResponse:
            return nikos::messaging::DeliveryKind::PresetResponse;
        case MessageType::Ring:
            return nikos::messaging::DeliveryKind::Ring;
        case MessageType::PresetMessage:
        default:
            return nikos::messaging::DeliveryKind::PresetMessage;
    }
}

}  // namespace

namespace nikos::messaging {

Service::Service(radio::RadioService& radio)
    : radio_(radio)
{
}

bool Service::begin(const Config& config)
{
    if (started_) {
        return !faulted_;
    }

    if (config.presence_interval_ms == 0
        || config.retry_interval_ms == 0
        || config.max_send_attempts == 0
        || config.delivery_timeout_ms == 0
        || config.tx_result_timeout_ms == 0
        || config.foreground_rx.interval_ms == 0
        || config.foreground_rx.wake_window_ms == 0
        || config.foreground_rx.wake_window_ms > config.foreground_rx.interval_ms
        || config.foreground_rx.reachability_timeout_ms == 0
        || config.background_rx.interval_ms == 0
        || config.background_rx.wake_window_ms == 0
        || config.background_rx.wake_window_ms > config.background_rx.interval_ms
        || config.background_rx.reachability_timeout_ms == 0) {
        ESP_LOGE(kTag, "Invalid messaging configuration");
        return false;
    }

    config_ = config;
    rx_profile_ = RxProfile::Background;

    ESP_LOGI(
        kTag,
        "Delivery policy retry=%lums jitter<=%ums max_attempts=%u timeout=%lums tx_result_guard=%ums ack_grace_margin=%ums",
        static_cast<unsigned long>(config_.retry_interval_ms),
        static_cast<unsigned>(config_.retry_jitter_ms),
        static_cast<unsigned>(config_.max_send_attempts),
        static_cast<unsigned long>(config_.delivery_timeout_ms),
        static_cast<unsigned>(config_.tx_result_timeout_ms),
        static_cast<unsigned>(config_.mac_success_ack_grace_margin_ms));

    started_ = true;
    if (!start_transport()) {
        started_ = false;
        return false;
    }

    // Seed logical IDs only after Wi-Fi/radio startup, when RF entropy is available.
    next_message_id_ = esp_random();
    if (next_message_id_ == 0) {
        next_message_id_ = 1;
    }

    return true;
}

bool Service::stop()
{
    bool stopped = true;
    if (transport_active_) {
        stopped = radio_.stop();
    }

    started_ = false;
    transport_active_ = false;
    faulted_ = false;
    rx_profile_ = RxProfile::Background;

    peer_known_ = false;
    peer_mac_ = {};
    last_peer_rx_ms_ = 0;
    latest_peer_rssi_ = 0;
    latest_peer_rssi_valid_ = false;
    last_presence_tx_ms_ = 0;
    current_presence_delay_ms_ = 0;

    next_message_id_ = 1;
    outgoing_ = OutgoingState{};
    unicast_in_flight_ = UnicastInFlight{};
    traffic_ = TrafficCounters{};

    pending_ack_references_ = {};
    pending_ack_head_ = 0;
    pending_ack_count_ = 0;

    recent_received_ids_ = {};
    recent_received_count_ = 0;
    recent_received_next_ = 0;

    incoming_queue_ = {};
    incoming_head_ = 0;
    incoming_count_ = 0;

    delivery_receipt_ = DeliveryReceipt{};
    delivery_ready_ = false;

    return stopped;
}

void Service::update()
{
    if (!started_ || !transport_active_) {
        return;
    }

    process_radio_events();

    std::uint32_t now = now_ms();

    if (unicast_in_flight_.active
        && now - unicast_in_flight_.submitted_ms
            >= config_.tx_result_timeout_ms) {
        handle_missing_tx_result(now, true);
        if (!transport_active_) {
            return;
        }
        now = now_ms();
    }

    if (outgoing_.active
        && now - outgoing_.started_ms >= config_.delivery_timeout_ms) {
        finish_outgoing(DeliveryOutcome::Failed, now);
    }

    if (last_presence_tx_ms_ == 0
        || now - last_presence_tx_ms_ >= current_presence_delay_ms_) {
        send_presence(now);
    }

    service_unicast(now);
}

bool Service::pause_transport()
{
    if (!started_ || !transport_active_) {
        return true;
    }

    // Drain callbacks already queued before deliberately handing the radio
    // to RadioLab. Any accepted unicast still unresolved after this drain
    // cannot retain its callback across radio_.stop(), so resolve it
    // conservatively as a missing TxResult before freezing logical clocks.
    process_radio_events();

    const std::uint32_t paused_at_ms = now_ms();
    if (unicast_in_flight_.active) {
        handle_missing_tx_result(paused_at_ms, false);
    }

    const bool stopped = radio_.stop();
    transport_active_ = false;

    if (outgoing_.active && outgoing_.paused_since_ms == 0) {
        outgoing_.paused_since_ms = paused_at_ms;
    }

    return stopped;
}

bool Service::resume_transport()
{
    if (!started_ || faulted_) {
        return false;
    }
    if (transport_active_) {
        return true;
    }

    if (!start_transport()) {
        return false;
    }

    if (outgoing_.active && outgoing_.paused_since_ms != 0) {
        const std::uint32_t resumed_at_ms = now_ms();
        const std::uint32_t paused_duration_ms =
            resumed_at_ms - outgoing_.paused_since_ms;

        outgoing_.started_ms += paused_duration_ms;
        if (outgoing_.last_send_ms != 0) {
            outgoing_.last_send_ms += paused_duration_ms;
        }
        outgoing_.paused_since_ms = 0;
    }

    return true;
}

bool Service::transport_active() const
{
    return transport_active_;
}

bool Service::set_rx_profile(RxProfile profile)
{
    if (!started_) {
        rx_profile_ = profile;
        return true;
    }

    if (!transport_active_) {
        rx_profile_ = profile;
        return true;
    }

    const RxProfile previous = rx_profile_;
    rx_profile_ = profile;
    if (!radio_.set_rx_power(rx_power_for(profile))) {
        rx_profile_ = previous;
        return false;
    }

    return true;
}

RxProfile Service::rx_profile() const
{
    return rx_profile_;
}

bool Service::set_radio_mode(radio::Mode mode)
{
    if (mode == config_.mode) {
        return true;
    }

    if (!started_ || !transport_active_) {
        config_.mode = mode;

        peer_known_ = false;
        peer_mac_ = {};
        last_peer_rx_ms_ = 0;
        latest_peer_rssi_ = 0;
        latest_peer_rssi_valid_ = false;
        last_presence_tx_ms_ = 0;
        current_presence_delay_ms_ = 0;
        outgoing_.last_send_ms = 0;
        return true;
    }

    if (!radio_.set_mode(mode)) {
        return false;
    }

    config_.mode = mode;
    radio_.clear_peer();

    // Drop only raw transport events that may have been received before the
    // protocol switch. Messaging queues/dedupe/delivery state remain intact.
    radio::Event stale_event;
    while (radio_.poll(stale_event)) {
    }

    peer_known_ = false;
    peer_mac_ = {};
    last_peer_rx_ms_ = 0;
    latest_peer_rssi_ = 0;
    latest_peer_rssi_valid_ = false;

    // Force fresh discovery in the selected mode on the next update().
    last_presence_tx_ms_ = 0;
    current_presence_delay_ms_ = 0;

    // Preserve the logical outgoing message and MessageId. It may retry as
    // soon as a compatible peer is rediscovered.
    outgoing_.last_send_ms = 0;

    return true;
}

radio::Mode Service::radio_mode() const
{
    return config_.mode;
}

bool Service::send_preset_message(std::uint16_t message_id)
{
    return start_outgoing(
        communicator_protocol::MessageType::PresetMessage,
        message_id,
        0);
}

bool Service::send_preset_response(
    std::uint16_t response_id,
    std::uint32_t reference_message_id)
{
    return start_outgoing(
        communicator_protocol::MessageType::PresetResponse,
        response_id,
        reference_message_id);
}

bool Service::send_ring()
{
    return start_outgoing(
        communicator_protocol::MessageType::Ring,
        0,
        0);
}

bool Service::outgoing_pending() const
{
    return outgoing_.active;
}

std::uint32_t Service::outgoing_logical_message_id() const
{
    return outgoing_.active ? outgoing_.message.message_id : 0;
}

bool Service::peer_known() const
{
    return peer_known_;
}

bool Service::peer_reachable() const
{
    if (faulted_) {
        return false;
    }

    const RxSchedule& schedule =
        rx_profile_ == RxProfile::Foreground
            ? config_.foreground_rx
            : config_.background_rx;

    return peer_known_
        && last_peer_rx_ms_ != 0
        && now_ms() - last_peer_rx_ms_
            <= schedule.reachability_timeout_ms;
}

const radio::MacAddress& Service::self_mac() const
{
    return radio_.self_mac();
}

const radio::MacAddress& Service::peer_mac() const
{
    return peer_mac_;
}

bool Service::latest_peer_rssi(std::int8_t& rssi) const
{
    if (!latest_peer_rssi_valid_) {
        return false;
    }

    rssi = latest_peer_rssi_;
    return true;
}

bool Service::peek_incoming(IncomingMessage& message) const
{
    if (incoming_count_ == 0) {
        return false;
    }

    message = incoming_queue_[incoming_head_];
    return true;
}

bool Service::consume_incoming(std::uint32_t logical_message_id)
{
    if (incoming_count_ == 0
        || incoming_queue_[incoming_head_].logical_message_id
            != logical_message_id) {
        return false;
    }

    incoming_head_ = (incoming_head_ + 1U) % kIncomingQueueDepth;
    --incoming_count_;
    return true;
}

bool Service::poll_delivery(DeliveryReceipt& receipt)
{
    if (!delivery_ready_) {
        return false;
    }

    receipt = delivery_receipt_;
    delivery_ready_ = false;
    return true;
}

bool Service::start_transport()
{
    if (!radio_.begin(config_.channel, config_.mode, rx_power_for(rx_profile_))) {
        return false;
    }

    if (peer_known_ && !radio_.set_peer(peer_mac_)) {
        radio_.stop();
        return false;
    }

    transport_active_ = true;
    last_presence_tx_ms_ = 0;
    current_presence_delay_ms_ = 0;
    return true;
}

radio::RxPowerConfig Service::rx_power_for(RxProfile profile) const
{
    const RxSchedule& schedule = rx_schedule_for(profile);

    radio::RxPowerConfig power;
    power.mode = radio::RxPowerMode::DutyCycled;
    power.wake_interval_ms = schedule.interval_ms;
    power.wake_window_ms = schedule.wake_window_ms;
    return power;
}

const RxSchedule& Service::rx_schedule_for(RxProfile profile) const
{
    return profile == RxProfile::Foreground
        ? config_.foreground_rx
        : config_.background_rx;
}

void Service::process_radio_events()
{
    radio::Event event;
    while (radio_.poll(event)) {
        if (event.type == radio::EventType::Rx) {
            process_rx(event.rx);
        } else if (event.type == radio::EventType::TxResult) {
            process_tx_result(event.tx);
        }
    }
}

void Service::process_tx_result(const radio::TxEvent& event)
{
    if (!unicast_in_flight_.active
        || !radio::mac_equal(
            event.destination,
            unicast_in_flight_.destination)) {
        // Broadcast PRESENCE callbacks and any unrelated/stale result are
        // intentionally outside messaging-unicast attribution.
        return;
    }

    resolve_in_flight(event.success, now_ms());
}

void Service::process_rx(const radio::RxEvent& event)
{
    if (radio::mac_equal(event.source, radio_.self_mac())) {
        return;
    }

    communicator_protocol::Message message;
    if (!communicator_protocol::decode(
            event.data.data(),
            event.length,
            message)) {
        return;
    }

    if (!peer_known_) {
        if (!radio_.set_peer(event.source)) {
            return;
        }

        peer_known_ = true;
        peer_mac_ = event.source;
        ESP_LOGI(
            kTag,
            "Learned messaging peer %02X:%02X:%02X:%02X:%02X:%02X",
            peer_mac_[0],
            peer_mac_[1],
            peer_mac_[2],
            peer_mac_[3],
            peer_mac_[4],
            peer_mac_[5]);
    } else if (!radio::mac_equal(event.source, peer_mac_)) {
        return;
    }

    record_peer_rx(event);

    if (message.type == communicator_protocol::MessageType::Presence) {
        return;
    }

    if (message.type == communicator_protocol::MessageType::Ack) {
        if (outgoing_.active
            && message.reference_id == outgoing_.message.message_id) {
            const std::uint32_t completed_ms =
                static_cast<std::uint32_t>(
                    event.received_time_us / 1000U);
            const DeliveryOutcome outcome =
                completed_ms - outgoing_.started_ms
                    < config_.delivery_timeout_ms
                ? DeliveryOutcome::Delivered
                : DeliveryOutcome::Failed;
            finish_outgoing(outcome, completed_ms);
        }
        return;
    }

    if (!is_notifiable(message.type)) {
        return;
    }

    if (is_duplicate(message.message_id)) {
        // Duplicate copies are still ACKed but are not surfaced again.
        send_ack(message.message_id);
        return;
    }

    IncomingMessage incoming;
    incoming.kind = to_incoming_kind(message.type);
    incoming.logical_message_id = message.message_id;
    incoming.reference_message_id = message.reference_id;
    incoming.value_id = message.value_id;

    // Do not application-ACK/dedupe a new logical message until it has been
    // retained locally. If this small queue is full, the sender will retry.
    if (!enqueue_incoming(incoming)) {
        return;
    }

    remember_received(message.message_id);
    send_ack(message.message_id);
}

void Service::record_peer_rx(const radio::RxEvent& event)
{
    last_peer_rx_ms_ =
        static_cast<std::uint32_t>(event.received_time_us / 1000U);

    latest_peer_rssi_valid_ = event.has_rssi;
    if (event.has_rssi) {
        latest_peer_rssi_ = event.rssi;
    }
}

void Service::send_presence(std::uint32_t now_ms)
{
    communicator_protocol::Message message;
    message.type = communicator_protocol::MessageType::Presence;

    std::array<std::uint8_t, communicator_protocol::kWireSize> wire{};
    if (communicator_protocol::encode(message, wire.data(), wire.size())
        && radio_.send_broadcast(wire.data(), wire.size())) {
        ++traffic_.presence_tx_submissions;
    }

    const std::uint32_t jitter =
        config_.presence_jitter_ms == 0
            ? 0
            : esp_random()
                % (static_cast<std::uint32_t>(config_.presence_jitter_ms) + 1U);
    current_presence_delay_ms_ = config_.presence_interval_ms + jitter;
    last_presence_tx_ms_ = now_ms;
}

void Service::send_ack(std::uint32_t reference_message_id)
{
    (void)enqueue_ack(reference_message_id);
}

bool Service::enqueue_ack(std::uint32_t reference_message_id)
{
    if (!peer_known_ || reference_message_id == 0) {
        return false;
    }

    if (pending_ack_count_ == kPendingAckDepth) {
        ++traffic_.ack_queue_overflows;
        ESP_LOGW(
            kTag,
            "Pending ACK queue full; dropping ref=%lu (sender retry/dedupe will recover)",
            static_cast<unsigned long>(reference_message_id));
        return false;
    }

    const std::size_t tail =
        (pending_ack_head_ + pending_ack_count_) % kPendingAckDepth;
    pending_ack_references_[tail] = reference_message_id;
    ++pending_ack_count_;
    return true;
}

bool Service::submit_next_ack(std::uint32_t now_ms)
{
    if (pending_ack_count_ == 0
        || unicast_in_flight_.active
        || !peer_known_) {
        return false;
    }

    const std::uint32_t reference_message_id =
        pending_ack_references_[pending_ack_head_];
    pending_ack_head_ =
        (pending_ack_head_ + 1U) % kPendingAckDepth;
    --pending_ack_count_;

    communicator_protocol::Message message;
    message.type = communicator_protocol::MessageType::Ack;
    message.message_id = next_logical_message_id();
    message.reference_id = reference_message_id;

    std::array<std::uint8_t, communicator_protocol::kWireSize> wire{};
    if (!communicator_protocol::encode(
            message,
            wire.data(),
            wire.size())) {
        ESP_LOGE(
            kTag,
            "Failed to encode application ACK ref=%lu",
            static_cast<unsigned long>(reference_message_id));
        return false;
    }

    if (!radio_.send_peer(wire.data(), wire.size())) {
        ++traffic_.ack_send_request_failures;
        ESP_LOGW(
            kTag,
            "Application ACK send request rejected ref=%lu",
            static_cast<unsigned long>(reference_message_id));
        return false;
    }

    ++traffic_.ack_tx_submissions;
    unicast_in_flight_.active = true;
    unicast_in_flight_.kind = UnicastKind::ApplicationAck;
    unicast_in_flight_.destination = peer_mac_;
    unicast_in_flight_.submitted_ms = now_ms;
    unicast_in_flight_.logical_message_id = reference_message_id;
    return true;
}

bool Service::start_outgoing(
    communicator_protocol::MessageType type,
    std::uint16_t value_id,
    std::uint32_t reference_message_id)
{
    if (!started_
        || faulted_
        || !peer_known_
        || outgoing_.active
        || outgoing_.completion_pending_transport) {
        return false;
    }

    outgoing_ = OutgoingState{};
    outgoing_.active = true;
    outgoing_.message.type = type;
    outgoing_.message.message_id = next_logical_message_id();
    outgoing_.message.reference_id = reference_message_id;
    outgoing_.message.value_id = value_id;
    outgoing_.started_ms = now_ms();

    if (transport_active_) {
        service_unicast(outgoing_.started_ms);
    } else {
        // A logical delivery created during an intentional transport handoff
        // starts with its delivery/retry clocks paused.
        outgoing_.paused_since_ms = outgoing_.started_ms;
    }

    return true;
}

void Service::send_outgoing(std::uint32_t now_ms)
{
    if (!transport_active_
        || !outgoing_.active
        || unicast_in_flight_.active
        || !peer_known_
        || !peer_reachable()) {
        return;
    }

    std::array<std::uint8_t, communicator_protocol::kWireSize> wire{};
    if (!communicator_protocol::encode(
            outgoing_.message,
            wire.data(),
            wire.size())) {
        ESP_LOGE(
            kTag,
            "Failed to encode logical delivery id=%lu",
            static_cast<unsigned long>(outgoing_.message.message_id));
        finish_outgoing(DeliveryOutcome::Failed, now_ms);
        return;
    }

    ++outgoing_.attempts;
    outgoing_.last_send_ms = now_ms;

    if (!radio_.send_peer(wire.data(), wire.size())) {
        ++outgoing_.send_request_failures;
        outgoing_.retry_delay_ms = next_retry_delay_ms();
        return;
    }

    ++outgoing_.accepted_submissions;
    ++traffic_.logical_payload_tx_submissions;

    unicast_in_flight_.active = true;
    unicast_in_flight_.kind = UnicastKind::OutgoingPayload;
    unicast_in_flight_.destination = peer_mac_;
    unicast_in_flight_.submitted_ms = now_ms;
    unicast_in_flight_.logical_message_id =
        outgoing_.message.message_id;
}

void Service::service_unicast(std::uint32_t now_ms)
{
    if (!transport_active_ || unicast_in_flight_.active) {
        return;
    }

    const bool outgoing_retry_due =
        outgoing_.active
        && (outgoing_.last_send_ms == 0
            || now_ms - outgoing_.last_send_ms
                >= outgoing_.retry_delay_ms);

    // Attempt-budget completion is not a transmission, so it is resolved
    // before choosing the next unicast submission.
    if (outgoing_retry_due
        && outgoing_.attempts >= config_.max_send_attempts) {
        finish_outgoing(DeliveryOutcome::Failed, now_ms);
    }

    if (unicast_in_flight_.active) {
        return;
    }

    // Receiver ACKs have priority over a due outgoing payload attempt.
    if (pending_ack_count_ > 0 && peer_known_) {
        (void)submit_next_ack(now_ms);
        return;
    }

    if (outgoing_.active
        && outgoing_retry_due
        && peer_reachable()) {
        send_outgoing(now_ms);
    }
}

void Service::handle_missing_tx_result(
    std::uint32_t now_ms,
    bool restart_transport)
{
    if (!unicast_in_flight_.active) {
        return;
    }

    const UnicastInFlight missing = unicast_in_flight_;
    unicast_in_flight_ = UnicastInFlight{};

    if (missing.kind == UnicastKind::OutgoingPayload) {
        ++traffic_.logical_tx_result_timeouts;

        if (outgoing_.message.message_id == missing.logical_message_id) {
            ++outgoing_.tx_result_timeouts;
            if (outgoing_.active) {
                outgoing_.last_send_ms = now_ms;
                outgoing_.retry_delay_ms = next_retry_delay_ms();
            }
        }

        ESP_LOGW(
            kTag,
            "Outgoing TxResult missing id=%lu reason=%s",
            static_cast<unsigned long>(missing.logical_message_id),
            restart_transport ? "guard_timeout" : "transport_pause");
    } else {
        ++traffic_.ack_tx_result_timeouts;
        ESP_LOGW(
            kTag,
            "Application ACK TxResult missing ref=%lu reason=%s",
            static_cast<unsigned long>(missing.logical_message_id),
            restart_transport ? "guard_timeout" : "transport_pause");
    }

    if (outgoing_.completion_pending_transport
        && outgoing_.message.message_id == missing.logical_message_id) {
        finalize_outgoing_metrics();
    }

    if (!restart_transport) {
        return;
    }

    // A timed-out callback cannot be safely distinguished from a later
    // unicast callback using destination MAC alone. Resetting the messaging
    // radio transport is the attribution barrier: unregister/deinit clears
    // the old callback/event queue before any newer unicast is submitted.
    // Preserve the existing Presence schedule across this exceptional reset.
    const std::uint32_t saved_last_presence_tx_ms =
        last_presence_tx_ms_;
    const std::uint32_t saved_presence_delay_ms =
        current_presence_delay_ms_;

    const bool stopped = radio_.stop();
    transport_active_ = false;
    if (!stopped) {
        ESP_LOGW(
            kTag,
            "Radio cleanup reported errors during TxResult recovery");
    }

    if (!start_transport()) {
        ESP_LOGE(
            kTag,
            "Radio restart failed during TxResult recovery; messaging faulted");

        // Fail closed. This is an internal attribution-barrier recovery
        // failure, not an intentional RadioLab pause. No work from the broken
        // transport lifecycle may be emitted later.
        transport_active_ = false;
        faulted_ = true;
        unicast_in_flight_ = UnicastInFlight{};
        pending_ack_references_ = {};
        pending_ack_head_ = 0;
        pending_ack_count_ = 0;

        peer_known_ = false;
        peer_mac_ = {};
        last_peer_rx_ms_ = 0;
        latest_peer_rssi_ = 0;
        latest_peer_rssi_valid_ = false;

        if (outgoing_.active) {
            finish_outgoing(DeliveryOutcome::Failed, now_ms);
        }
        return;
    }

    last_presence_tx_ms_ = saved_last_presence_tx_ms;
    current_presence_delay_ms_ = saved_presence_delay_ms;
}

void Service::resolve_in_flight(
    bool success,
    std::uint32_t resolved_ms)
{
    if (!unicast_in_flight_.active) {
        return;
    }

    const UnicastInFlight resolved = unicast_in_flight_;
    unicast_in_flight_ = UnicastInFlight{};

    if (resolved.kind == UnicastKind::ApplicationAck) {
        if (success) {
            ++traffic_.ack_mac_successes;
        } else {
            ++traffic_.ack_mac_failures;
        }
        return;
    }

    if (outgoing_.message.message_id != resolved.logical_message_id) {
        ESP_LOGW(
            kTag,
            "Ignoring TxResult for stale logical id=%lu",
            static_cast<unsigned long>(resolved.logical_message_id));
        return;
    }

    if (success) {
        ++outgoing_.mac_successes;
        ++traffic_.logical_mac_successes;
        if (outgoing_.active) {
            outgoing_.last_send_ms = resolved_ms;
            outgoing_.retry_delay_ms = mac_success_ack_grace_ms();
        }
    } else {
        ++outgoing_.mac_failures;
        ++traffic_.logical_mac_failures;
        if (outgoing_.active) {
            outgoing_.last_send_ms = resolved_ms;
            outgoing_.retry_delay_ms = next_retry_delay_ms();
        }
    }

    if (outgoing_.completion_pending_transport) {
        finalize_outgoing_metrics();
    }
}

std::uint32_t Service::next_retry_delay_ms() const
{
    const std::uint32_t jitter =
        config_.retry_jitter_ms == 0
            ? 0
            : esp_random()
                % (static_cast<std::uint32_t>(config_.retry_jitter_ms) + 1U);
    return config_.retry_interval_ms + jitter;
}

std::uint32_t Service::mac_success_ack_grace_ms() const
{
    return static_cast<std::uint32_t>(
        rx_schedule_for(rx_profile_).interval_ms)
        + config_.mac_success_ack_grace_margin_ms;
}

void Service::finish_outgoing(
    DeliveryOutcome outcome,
    std::uint32_t completed_ms)
{
    if (!outgoing_.active) {
        return;
    }

    delivery_receipt_.kind = to_delivery_kind(outgoing_.message.type);
    delivery_receipt_.outcome = outcome;
    delivery_receipt_.logical_message_id = outgoing_.message.message_id;
    delivery_receipt_.send_attempt_count = outgoing_.attempts;
    delivery_receipt_.send_request_failure_count =
        outgoing_.send_request_failures;
    delivery_receipt_.latency_ms =
        completed_ms - outgoing_.started_ms;
    delivery_ready_ = true;

    if (outcome == DeliveryOutcome::Delivered) {
        ++traffic_.delivered_logical_messages;
    } else {
        ++traffic_.failed_logical_messages;
    }

    outgoing_.active = false;
    outgoing_.completed_outcome = outcome;
    outgoing_.completed_ms = completed_ms;
    outgoing_.completion_pending_transport =
        unicast_in_flight_.active
        && unicast_in_flight_.kind == UnicastKind::OutgoingPayload
        && unicast_in_flight_.logical_message_id
            == outgoing_.message.message_id;

    if (!outgoing_.completion_pending_transport) {
        finalize_outgoing_metrics();
    }
}

void Service::finalize_outgoing_metrics()
{
    if (outgoing_.message.message_id == 0) {
        return;
    }

    ESP_LOGI(
        kTag,
        "Logical delivery id=%lu kind=%s outcome=%s attempts=%lu accepted=%lu send_request_failures=%lu mac_success=%lu mac_fail=%lu tx_result_missing=%lu latency_ms=%lu",
        static_cast<unsigned long>(outgoing_.message.message_id),
        delivery_kind_name(to_delivery_kind(outgoing_.message.type)),
        delivery_outcome_name(outgoing_.completed_outcome),
        static_cast<unsigned long>(outgoing_.attempts),
        static_cast<unsigned long>(outgoing_.accepted_submissions),
        static_cast<unsigned long>(outgoing_.send_request_failures),
        static_cast<unsigned long>(outgoing_.mac_successes),
        static_cast<unsigned long>(outgoing_.mac_failures),
        static_cast<unsigned long>(outgoing_.tx_result_timeouts),
        static_cast<unsigned long>(
            outgoing_.completed_ms - outgoing_.started_ms));
    ESP_LOGI(
        kTag,
        "Traffic submissions logical=%lu ack=%lu ack_request_fail=%lu presence=%lu logical_mac_success=%lu logical_mac_fail=%lu logical_tx_missing=%lu ack_mac_success=%lu ack_mac_fail=%lu ack_tx_missing=%lu ack_queue_overflow=%lu delivered=%lu failed=%lu",
        static_cast<unsigned long>(
            traffic_.logical_payload_tx_submissions),
        static_cast<unsigned long>(traffic_.ack_tx_submissions),
        static_cast<unsigned long>(
            traffic_.ack_send_request_failures),
        static_cast<unsigned long>(traffic_.presence_tx_submissions),
        static_cast<unsigned long>(traffic_.logical_mac_successes),
        static_cast<unsigned long>(traffic_.logical_mac_failures),
        static_cast<unsigned long>(
            traffic_.logical_tx_result_timeouts),
        static_cast<unsigned long>(traffic_.ack_mac_successes),
        static_cast<unsigned long>(traffic_.ack_mac_failures),
        static_cast<unsigned long>(traffic_.ack_tx_result_timeouts),
        static_cast<unsigned long>(traffic_.ack_queue_overflows),
        static_cast<unsigned long>(
            traffic_.delivered_logical_messages),
        static_cast<unsigned long>(traffic_.failed_logical_messages));

    outgoing_ = OutgoingState{};
}

bool Service::is_duplicate(std::uint32_t logical_message_id) const
{
    if (logical_message_id == 0) {
        return false;
    }

    for (std::size_t index = 0; index < recent_received_count_; ++index) {
        if (recent_received_ids_[index] == logical_message_id) {
            return true;
        }
    }

    return false;
}

void Service::remember_received(std::uint32_t logical_message_id)
{
    if (logical_message_id == 0) {
        return;
    }

    recent_received_ids_[recent_received_next_] = logical_message_id;
    recent_received_next_ = (recent_received_next_ + 1U) % kDedupeDepth;
    if (recent_received_count_ < kDedupeDepth) {
        ++recent_received_count_;
    }
}

bool Service::enqueue_incoming(const IncomingMessage& message)
{
    if (incoming_count_ == kIncomingQueueDepth) {
        return false;
    }

    const std::size_t tail =
        (incoming_head_ + incoming_count_) % kIncomingQueueDepth;
    incoming_queue_[tail] = message;
    ++incoming_count_;
    return true;
}

std::uint32_t Service::next_logical_message_id()
{
    std::uint32_t current = next_message_id_++;
    if (current == 0) {
        current = next_message_id_++;
    }
    if (next_message_id_ == 0) {
        ++next_message_id_;
    }
    return current;
}

std::uint32_t Service::now_ms() const
{
    return static_cast<std::uint32_t>(esp_timer_get_time() / 1000U);
}

}  // namespace nikos::messaging
