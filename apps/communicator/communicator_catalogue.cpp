#include "communicator/communicator_catalogue.hpp"

namespace nikos::communicator::catalogue {

const char* preset_text(PresetId id)
{
    switch (id) {
        case PresetId::Greeting:
            return "CZEŚĆ!";
        case PresetId::CanTalk:
            return "MOŻESZ GADAĆ?";
        case PresetId::Walk:
            return "IDZIESZ NA SPACER?";
        case PresetId::Cans:
            return "MASZ PUSZKI?";
        case PresetId::Wait:
            return "ZACZEKAĆ?";
        default:
            return "?";
    }
}

const char* response_text(ResponseId id)
{
    switch (id) {
        case ResponseId::GreetingHello:
            return "Cześć!";
        case ResponseId::YesComing:
            return "Tak, już wychodzę";
        case ResponseId::Soon:
            return "Za chwilę";
        case ResponseId::Busy:
            return "Teraz jestem zajęty";
        case ResponseId::Later:
            return "Później";
        case ResponseId::NotToday:
            return "Nie dzisiaj";
        case ResponseId::Have:
            return "Mam";
        case ResponseId::DontHave:
            return "Nie mam";
        case ResponseId::WillCheck:
            return "Sprawdzę";
        case ResponseId::YesWait:
            return "Tak, zaczekaj";
        case ResponseId::DontWait:
            return "Nie czekaj";
        case ResponseId::HumanOk:
            return "OK";
        default:
            return "?";
    }
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
        case ResponseId::HumanOk:
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
