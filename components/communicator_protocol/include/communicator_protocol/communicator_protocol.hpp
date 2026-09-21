#pragma once

#include <cstddef>
#include <cstdint>

namespace nikos::communicator_protocol {

constexpr std::uint8_t kVersion = 2;
constexpr std::size_t kHeaderSize = 2;
constexpr std::size_t kPresenceWireSize = 2;
constexpr std::size_t kRingWireSize = 6;
constexpr std::size_t kAckWireSize = 6;
constexpr std::size_t kPresetMessageWireSize = 7;
constexpr std::size_t kPresetResponseWireSize = 11;
constexpr std::size_t kMaxWireSize = kPresetResponseWireSize;

enum class MessageType : std::uint8_t {
    Presence = 1,
    PresetMessage = 2,
    PresetResponse = 3,
    Ack = 4,
    Ring = 5,
};

struct Message {
    MessageType type = MessageType::Presence;
    std::uint32_t message_id = 0;
    std::uint32_t reference_id = 0;
    std::uint16_t value_id = 0;
};

bool encode(
    const Message& message,
    std::uint8_t* output,
    std::size_t capacity,
    std::size_t& encoded_length);
bool decode(const std::uint8_t* data, std::size_t length, Message& message);

}  // namespace nikos::communicator_protocol
