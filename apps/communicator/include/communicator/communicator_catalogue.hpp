#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

namespace nikos::communicator::catalogue {

enum class PresetId : std::uint16_t {
    Greeting = 1,
    CanTalk = 2,
    Walk = 3,
    Cans = 4,
    Wait = 5,
};

enum class ResponseId : std::uint16_t {
    GreetingHello = 1,
    YesComing = 2,
    Soon = 3,
    Busy = 4,
    Later = 5,
    NotToday = 6,
    Have = 7,
    DontHave = 8,
    WillCheck = 9,
    YesWait = 10,
    DontWait = 11,
};

struct ResponseSet {
    std::array<ResponseId, 3> ids{};
    std::uint8_t count = 0;
};

constexpr std::array<PresetId, 5> kPresetOrder = {
    PresetId::Greeting,
    PresetId::CanTalk,
    PresetId::Walk,
    PresetId::Cans,
    PresetId::Wait,
};

const char* preset_text(PresetId id);
const char* response_text(ResponseId id);

bool preset_from_wire(std::uint16_t raw, PresetId& id);
bool response_from_wire(std::uint16_t raw, ResponseId& id);

ResponseSet responses_for(PresetId preset);
bool response_allowed_for(PresetId preset, ResponseId response);
bool needs_wait_decision(ResponseId response);

}  // namespace nikos::communicator::catalogue
