#include "communicator_protocol/communicator_protocol.hpp"

namespace {

constexpr std::uint8_t kDiscriminator = 0xA7U;
constexpr std::uint8_t kVersionShift = 4U;
constexpr std::uint8_t kVersionMask = 0xF0U;
constexpr std::uint8_t kTypeMask = 0x0FU;

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

std::uint8_t header_type_byte(
    nikos::communicator_protocol::MessageType type)
{
    return static_cast<std::uint8_t>(
        (nikos::communicator_protocol::kVersion << kVersionShift)
        | (static_cast<std::uint8_t>(type) & kTypeMask));
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
    if (output == nullptr
        || !valid_type(raw_type)) {
        return false;
    }

    const std::size_t expected_length = wire_size_for(message.type);
    if (expected_length == 0 || capacity < expected_length) {
        return false;
    }

    if ((message.type == MessageType::PresetMessage
            || message.type == MessageType::PresetResponse)
        && message.value_id > 0xFFU) {
        return false;
    }

    output[0] = kDiscriminator;
    output[1] = header_type_byte(message.type);

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
            output[6] = static_cast<std::uint8_t>(message.value_id);
            break;

        case MessageType::PresetResponse:
            write_u32(output + kHeaderSize, message.message_id);
            write_u32(output + 6, message.reference_id);
            output[10] = static_cast<std::uint8_t>(message.value_id);
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
        || data[0] != kDiscriminator) {
        return false;
    }

    const std::uint8_t raw_version =
        static_cast<std::uint8_t>(
            (data[1] & kVersionMask) >> kVersionShift);
    const std::uint8_t raw_type =
        static_cast<std::uint8_t>(data[1] & kTypeMask);

    if (raw_version != kVersion || !valid_type(raw_type)) {
        return false;
    }

    const MessageType type = static_cast<MessageType>(raw_type);
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
            decoded.value_id = data[6];
            break;

        case MessageType::PresetResponse:
            decoded.message_id = read_u32(data + kHeaderSize);
            decoded.reference_id = read_u32(data + 6);
            decoded.value_id = data[10];
            break;

        default:
            return false;
    }

    message = decoded;
    return true;
}

}  // namespace nikos::communicator_protocol
