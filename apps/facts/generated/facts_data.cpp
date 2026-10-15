#include "facts_data.hpp"

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
            return "KOSMOS";
        case Category::Animals:
            return "ZWIERZĘTA";
        case Category::Technology:
            return "TECHNOLOGIA";
        case Category::Human:
            return "CZŁOWIEK";
        case Category::History:
            return "HISTORIA";
        case Category::World:
            return "ŚWIAT";
        default:
            return "KOSMOS";
    }
}

}  // namespace nikos::facts::data
