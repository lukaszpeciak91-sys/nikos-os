#pragma once

#include <cstddef>
#include <cstdint>

namespace nikos::facts::data {

enum class Category : std::uint8_t {
    Space,
    Animals,
    Technology,
    Human,
    History,
    World,
};

struct Fact {
    const char* id;
    const char* title;
    const char* text;
};

inline constexpr std::size_t kCategoryCount = 6;
inline constexpr std::size_t kFactsPerCategory = 100;
inline constexpr std::size_t kTotalFacts =
    kCategoryCount * kFactsPerCategory;

extern const Fact kSpaceFacts[kFactsPerCategory];
extern const Fact kAnimalsFacts[kFactsPerCategory];
extern const Fact kTechnologyFacts[kFactsPerCategory];
extern const Fact kHumanFacts[kFactsPerCategory];
extern const Fact kHistoryFacts[kFactsPerCategory];
extern const Fact kWorldFacts[kFactsPerCategory];

const Fact* category_facts(Category category);
const char* category_label(Category category);

}  // namespace nikos::facts::data
