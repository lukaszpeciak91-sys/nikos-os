#include "facts/generated/facts_data.hpp"

namespace nikos::facts::data {

const Fact* category_facts(Category category)
{
    switch (category) {
        case Category::Space:
            return kSpaceFacts;
        case Category::Animals:
            return kAnimalsFacts;
        case Category::Technology:
            return kTechnologyFacts;
        case Category::Human:
            return kHumanFacts;
        case Category::History:
            return kHistoryFacts;
        case Category::World:
            return kWorldFacts;
        default:
            return kSpaceFacts;
    }
}

const char* category_label(Category category)
{
    switch (category) {
        case Category::Space:
            return u8"KOSMOS";
        case Category::Animals:
            return u8"ZWIERZĘTA";
        case Category::Technology:
            return u8"TECHNOLOGIA";
        case Category::Human:
            return u8"CZŁOWIEK";
        case Category::History:
            return u8"HISTORIA";
        case Category::World:
            return u8"ŚWIAT";
        default:
            return "KOSMOS";
    }
}

}  // namespace nikos::facts::data
