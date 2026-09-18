#pragma once

#include <cstddef>
#include <cstdint>

namespace nikos::protocol {

constexpr std::uint8_t kVersion = 1;
constexpr std::size_t kWireSize = 17;
constexpr std::int8_t kRssiUnavailable = 127;

enum class MessageType : std::uint8_t {
    Discovery = 1,
    Ping = 2,
    Ack = 3,
    Hello = 4,
};

enum class RadioMode : std::uint8_t {
    Normal = 0,
    Lr = 1,
};

struct Message {
    MessageType type = MessageType::Discovery;
    RadioMode mode = RadioMode::Normal;
    std::uint32_t sequence = 0;
    std::uint32_t reference_sequence = 0;
    std::int8_t reported_rssi = kRssiUnavailable;
};

bool encode(const Message& message, std::uint8_t* output, std::size_t output_size);
bool decode(const std::uint8_t* data, std::size_t data_size, Message& message);

const char* message_type_name(MessageType type);
const char* radio_mode_name(RadioMode mode);

}  // namespace nikos::protocol
