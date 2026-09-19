#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

namespace nikos::radio {

constexpr std::size_t kMacSize = 6;
constexpr std::size_t kMaxFrameSize = 64;

using MacAddress = std::array<std::uint8_t, kMacSize>;

enum class Mode : std::uint8_t {
    Normal,
    Lr,
};

enum class RxPowerMode : std::uint8_t {
    Continuous,
    DutyCycled,
};

struct RxPowerConfig {
    RxPowerMode mode = RxPowerMode::Continuous;
    std::uint16_t wake_interval_ms = 0;
    std::uint16_t wake_window_ms = 0;
};

enum class EventType : std::uint8_t {
    Rx,
    TxResult,
};

struct RxEvent {
    MacAddress source{};
    MacAddress destination{};
    std::array<std::uint8_t, kMaxFrameSize> data{};
    std::size_t length = 0;
    std::uint64_t received_time_us = 0;
    std::int8_t rssi = 0;
    bool has_rssi = false;
};

struct TxEvent {
    MacAddress destination{};
    bool success = false;
};

struct Event {
    EventType type = EventType::Rx;
    RxEvent rx{};
    TxEvent tx{};
};

class RadioService final {
public:
    bool begin(
        std::uint8_t channel,
        Mode mode,
        const RxPowerConfig& rx_power = RxPowerConfig{});
    bool stop();

    bool set_mode(Mode mode);
    Mode mode() const;
    std::uint8_t channel() const;

    bool set_rx_power(const RxPowerConfig& config);
    const RxPowerConfig& rx_power() const;

    const MacAddress& self_mac() const;

    bool set_peer(const MacAddress& mac);
    void clear_peer();
    bool has_peer() const;
    const MacAddress& peer_mac() const;

    bool send_broadcast(const std::uint8_t* data, std::size_t length);
    bool send_peer(const std::uint8_t* data, std::size_t length);

    bool poll(Event& event);
    std::uint32_t dropped_event_count() const;

private:
    bool initialize_network_platform();
    bool apply_mode(Mode mode);
    bool apply_rx_power(const RxPowerConfig& config);
    bool add_broadcast_peer();

    bool network_platform_initialized_ = false;
    bool wifi_initialized_ = false;
    bool wifi_started_ = false;
    bool esp_now_initialized_ = false;
    bool initialized_ = false;
    bool has_peer_ = false;
    std::uint8_t channel_ = 0;
    Mode mode_ = Mode::Normal;
    RxPowerConfig rx_power_{};
    MacAddress self_mac_{};
    MacAddress peer_mac_{};
};

bool mac_equal(const MacAddress& left, const MacAddress& right);
const char* mode_name(Mode mode);

}  // namespace nikos::radio
