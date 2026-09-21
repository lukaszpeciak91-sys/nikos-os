#include "communicator_protocol/communicator_protocol.hpp"

namespace {

void write_u32(std::uint8_t* output, std::uint32_t value)
{
    output[0] = static_cast<std::uint8_t>((value >> 24U) & 0xFFU);
    output[1] = static_cast<std::uint8_t>((value >> 16U) & 0xFFU);
    output[2] = static_cast<std::uint8_t>((value >> 8U) & 0xFFU);
    output[3] = static_cast<std::uint8_t>(value & 0xFFU);
}

std::uint32_t read_u32(const std::uint8_t* input)
{
    return (static_cast<std::uint32_t>(input[0]) << 24U)
        | (static_cast<std::uint32_t>(input[1]) << 16U)
        | (static_cast<std::uint32_t>(input[2]) << 8U)
        | static_cast<std::uint32_t>(input[3]);
}

void write_u16(std::uint8_t* output, std::uint16_t value)
{
    output[0] = static_cast<std::uint8_t>((value >> 8U) & 0xFFU);
    output[1] = static_cast<std::uint8_t>(value & 0xFFU);
}

std::uint16_t read_u16(const std::uint8_t* input)
{
    return static_cast<std::uint16_t>(
        (static_cast<std::uint16_t>(input[0]) << 8U)
        | static_cast<std::uint16_t>(input[1]));
}

bool valid_type(std::uint8_t raw_type)
{
    using nikos::communicator_protocol::MessageType;
    switch (static_cast<MessageType>(raw_type)) {
        case MessageType::Presence:
        case MessageType::PresetMessage:
        case MessageType::PresetResponse:
        case MessageType::Ack:
        case MessageType::Ring:
            return true;
        default:
            return false;
    }
}

std::size_t wire_size_for(nikos::communicator_protocol::MessageType type)
{
    using namespace nikos::communicator_protocol;
    switch (type) {
        case MessageType::Presence:
            return kPresenceWireSize;
        case MessageType::Ring:
            return kRingWireSize;
        case MessageType::Ack:
            return kAckWireSize;
        case MessageType::PresetMessage:
            return kPresetMessageWireSize;
        case MessageType::PresetResponse:
            return kPresetResponseWireSize;
        default:
            return 0;
    }
}

}  // namespace

namespace nikos::communicator_protocol {

bool encode(
    const Message& message,
    std::uint8_t* output,
    std::size_t capacity,
    std::size_t& encoded_length)
{
    encoded_length = 0;

    const std::uint8_t raw_type =
        static_cast<std::uint8_t>(message.type);
    if (output == nullptr || !valid_type(raw_type)) {
        return false;
    }

    const std::size_t expected_length = wire_size_for(message.type);
    if (expected_length == 0 || capacity < expected_length) {
        return false;
    }

    output[0] = kV2Discriminator;
    output[1] = raw_type;

    switch (message.type) {
        case MessageType::Presence:
            break;

        case MessageType::Ring:
            write_u32(output + kHeaderSize, message.message_id);
            break;

        case MessageType::Ack:
            write_u32(output + kHeaderSize, message.reference_id);
            break;

        case MessageType::PresetMessage:
            write_u32(output + kHeaderSize, message.message_id);
            write_u16(output + 6, message.value_id);
            break;

        case MessageType::PresetResponse:
            write_u32(output + kHeaderSize, message.message_id);
            write_u32(output + 6, message.reference_id);
            write_u16(output + 10, message.value_id);
            break;

        default:
            return false;
    }

    encoded_length = expected_length;
    return true;
}

bool decode(const std::uint8_t* data, std::size_t length, Message& message)
{
    if (data == nullptr
        || length < kHeaderSize
        || data[0] != kV2Discriminator
        || !valid_type(data[1])) {
        return false;
    }

    const MessageType type = static_cast<MessageType>(data[1]);
    const std::size_t expected_length = wire_size_for(type);
    if (length != expected_length) {
        return false;
    }

    Message decoded{};
    decoded.type = type;

    switch (type) {
        case MessageType::Presence:
            break;

        case MessageType::Ring:
            decoded.message_id = read_u32(data + kHeaderSize);
            break;

        case MessageType::Ack:
            decoded.reference_id = read_u32(data + kHeaderSize);
            break;

        case MessageType::PresetMessage:
            decoded.message_id = read_u32(data + kHeaderSize);
            decoded.value_id = read_u16(data + 6);
            break;

        case MessageType::PresetResponse:
            decoded.message_id = read_u32(data + kHeaderSize);
            decoded.reference_id = read_u32(data + 6);
            decoded.value_id = read_u16(data + 10);
            break;

        default:
            return false;
    }

    message = decoded;
    return true;
}

}  // namespace nikos::communicator_protocol
