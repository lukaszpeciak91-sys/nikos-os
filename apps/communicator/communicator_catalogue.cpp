#include "communicator/communicator_catalogue.hpp"

namespace nikos::communicator::catalogue {

const char* preset_text(PresetId id)
{
    switch (id) {
        case PresetId::Greeting:
            return "HEJ!";
        case PresetId::CanTalk:
            return "MASZ CZAS?";
        case PresetId::Walk:
            return "IDZIESZ NA SPACER?";
        case PresetId::Cans:
            return "MASZ PUSZKI?";
        case PresetId::Wait:
            return "CZEKAC?";
        default:
            return "?";
    }
}

const char* response_text(ResponseId id)
{
    switch (id) {
        case ResponseId::GreetingHello:
            return "HEJ!";
        case ResponseId::YesComing:
            return "TAK, ZARAZ";
        case ResponseId::Soon:
            return "ZA MOMENT";
        case ResponseId::Busy:
            return "NIE TERAZ";
        case ResponseId::Later:
            return "POTEM";
        case ResponseId::NotToday:
            return "NIE DZISIAJ";
        case ResponseId::Have:
            return "MAM";
        case ResponseId::DontHave:
            return "NIE MAM";
        case ResponseId::WillCheck:
            return "SPRAWDZAM";
        case ResponseId::YesWait:
            return "TAK";
        case ResponseId::DontWait:
            return "NIE";
        default:
            return "?";
    }
}

const char* response_text_for(
    PresetId preset,
    ResponseId response)
{
    if (preset == PresetId::CanTalk
        && response == ResponseId::YesComing) {
        return "TAK";
    }

    return response_text(response);
}

bool preset_from_wire(std::uint16_t raw, PresetId& id)
{
    switch (static_cast<PresetId>(raw)) {
        case PresetId::Greeting:
        case PresetId::CanTalk:
        case PresetId::Walk:
        case PresetId::Cans:
        case PresetId::Wait:
            id = static_cast<PresetId>(raw);
            return true;
        default:
            return false;
    }
}

bool response_from_wire(std::uint16_t raw, ResponseId& id)
{
    switch (static_cast<ResponseId>(raw)) {
        case ResponseId::GreetingHello:
        case ResponseId::YesComing:
        case ResponseId::Soon:
        case ResponseId::Busy:
        case ResponseId::Later:
        case ResponseId::NotToday:
        case ResponseId::Have:
        case ResponseId::DontHave:
        case ResponseId::WillCheck:
        case ResponseId::YesWait:
        case ResponseId::DontWait:
            id = static_cast<ResponseId>(raw);
            return true;
        default:
            return false;
    }
}

ResponseSet responses_for(PresetId preset)
{
    ResponseSet set;

    switch (preset) {
        case PresetId::Greeting:
            set.ids[0] = ResponseId::GreetingHello;
            set.count = 1;
            break;

        case PresetId::CanTalk:
            set.ids[0] = ResponseId::YesComing;
            set.ids[1] = ResponseId::Soon;
            set.ids[2] = ResponseId::Busy;
            set.count = 3;
            break;

        case PresetId::Walk:
            set.ids[0] = ResponseId::YesComing;
            set.ids[1] = ResponseId::Later;
            set.ids[2] = ResponseId::NotToday;
            set.count = 3;
            break;

        case PresetId::Cans:
            set.ids[0] = ResponseId::Have;
            set.ids[1] = ResponseId::DontHave;
            set.ids[2] = ResponseId::WillCheck;
            set.count = 3;
            break;

        case PresetId::Wait:
            set.ids[0] = ResponseId::YesWait;
            set.ids[1] = ResponseId::DontWait;
            set.count = 2;
            break;
    }

    return set;
}

bool response_allowed_for(PresetId preset, ResponseId response)
{
    const ResponseSet set = responses_for(preset);
    for (std::uint8_t index = 0; index < set.count; ++index) {
        if (set.ids[index] == response) {
            return true;
        }
    }
    return false;
}

bool needs_wait_decision(ResponseId response)
{
    return response == ResponseId::Soon
        || response == ResponseId::Later
        || response == ResponseId::WillCheck;
}

}  // namespace nikos::communicator::catalogue
