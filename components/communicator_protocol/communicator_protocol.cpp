#include "communicator_protocol/communicator_protocol.hpp"

namespace {

constexpr std::uint8_t kMagic[] = {'N', 'C', 'O', 'M'};

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

}  // namespace

namespace nikos::communicator_protocol {

bool encode(const Message& message, std::uint8_t* output, std::size_t output_size)
{
    if (output == nullptr
        || output_size < kWireSize
        || !valid_type(static_cast<std::uint8_t>(message.type))) {
        return false;
    }

    output[0] = kMagic[0];
    output[1] = kMagic[1];
    output[2] = kMagic[2];
    output[3] = kMagic[3];
    output[4] = kVersion;
    output[5] = static_cast<std::uint8_t>(message.type);
    output[6] = 0;
    output[7] = 0;
    write_u32(output + 8, message.message_id);
    write_u32(output + 12, message.reference_id);
    write_u16(output + 16, message.value_id);
    output[18] = 0;
    output[19] = 0;
    return true;
}

bool decode(const std::uint8_t* data, std::size_t length, Message& message)
{
    if (data == nullptr
        || length != kWireSize
        || data[0] != kMagic[0]
        || data[1] != kMagic[1]
        || data[2] != kMagic[2]
        || data[3] != kMagic[3]
        || data[4] != kVersion
        || !valid_type(data[5])) {
        return false;
    }

    message.type = static_cast<MessageType>(data[5]);
    message.message_id = read_u32(data + 8);
    message.reference_id = read_u32(data + 12);
    message.value_id = read_u16(data + 16);
    return true;
}

}  // namespace nikos::communicator_protocol
