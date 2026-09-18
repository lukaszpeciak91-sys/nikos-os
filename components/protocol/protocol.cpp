#include "protocol/protocol.hpp"

#include <array>

namespace {

constexpr std::array<std::uint8_t, 4> kMagic = {'N', 'R', 'L', 'B'};

void write_u32_be(std::uint8_t* output, std::uint32_t value)
{
    output[0] = static_cast<std::uint8_t>((value >> 24) & 0xFFU);
    output[1] = static_cast<std::uint8_t>((value >> 16) & 0xFFU);
    output[2] = static_cast<std::uint8_t>((value >> 8) & 0xFFU);
    output[3] = static_cast<std::uint8_t>(value & 0xFFU);
}

std::uint32_t read_u32_be(const std::uint8_t* input)
{
    return (static_cast<std::uint32_t>(input[0]) << 24)
        | (static_cast<std::uint32_t>(input[1]) << 16)
        | (static_cast<std::uint32_t>(input[2]) << 8)
        | static_cast<std::uint32_t>(input[3]);
}

bool valid_type(std::uint8_t value)
{
    return value >= static_cast<std::uint8_t>(nikos::protocol::MessageType::Discovery)
        && value <= static_cast<std::uint8_t>(nikos::protocol::MessageType::Hello);
}

bool valid_mode(std::uint8_t value)
{
    return value <= static_cast<std::uint8_t>(nikos::protocol::RadioMode::Lr);
}

}  // namespace

namespace nikos::protocol {

bool encode(const Message& message, std::uint8_t* output, std::size_t output_size)
{
    if (output == nullptr || output_size < kWireSize) {
        return false;
    }

    output[0] = kMagic[0];
    output[1] = kMagic[1];
    output[2] = kMagic[2];
    output[3] = kMagic[3];
    output[4] = kVersion;
    output[5] = static_cast<std::uint8_t>(message.type);
    output[6] = static_cast<std::uint8_t>(message.mode);
    output[7] = 0;
    write_u32_be(&output[8], message.sequence);
    write_u32_be(&output[12], message.reference_sequence);
    output[16] = static_cast<std::uint8_t>(message.reported_rssi);
    return true;
}

bool decode(const std::uint8_t* data, std::size_t data_size, Message& message)
{
    if (data == nullptr || data_size != kWireSize) {
        return false;
    }

    if (data[0] != kMagic[0]
        || data[1] != kMagic[1]
        || data[2] != kMagic[2]
        || data[3] != kMagic[3]
        || data[4] != kVersion
        || !valid_type(data[5])
        || !valid_mode(data[6])) {
        return false;
    }

    message.type = static_cast<MessageType>(data[5]);
    message.mode = static_cast<RadioMode>(data[6]);
    message.sequence = read_u32_be(&data[8]);
    message.reference_sequence = read_u32_be(&data[12]);
    message.reported_rssi = static_cast<std::int8_t>(data[16]);
    return true;
}

const char* message_type_name(MessageType type)
{
    switch (type) {
        case MessageType::Discovery:
            return "DISCOVERY";
        case MessageType::Ping:
            return "PING";
        case MessageType::Ack:
            return "ACK";
        case MessageType::Hello:
            return "HELLO";
        default:
            return "UNKNOWN";
    }
}

const char* radio_mode_name(RadioMode mode)
{
    return mode == RadioMode::Lr ? "LR" : "NORMAL";
}

}  // namespace nikos::protocol
