#pragma once
#include <cstdint>
namespace nikos::facts::quiz {
struct Item {
 const char* id;
 const char* category;
 const char* source_id;
 const char* statement;
 bool correct;
 const char* explanation;
};
inline constexpr std::uint16_t kItemCount = 120;
extern const Item kItems[kItemCount];
}
