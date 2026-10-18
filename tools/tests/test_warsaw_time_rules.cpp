// Host test: g++ -std=c++17 -Wall -Wextra -Werror -Icomponents/clock/include
//   tools/tests/test_warsaw_time_rules.cpp -o /tmp/warsaw_rules && /tmp/warsaw_rules
#include "clock/warsaw_time_rules.hpp"
#include <cassert>

using namespace nikos::clock::warsaw;
constexpr bool equal(DateTime a, DateTime b) {
    return a.year==b.year && a.month==b.month && a.day==b.day
        && a.hour==b.hour && a.minute==b.minute && a.second==b.second;
}
static_assert(last_sunday(2026,3)==29 && last_sunday(2026,10)==25);
static_assert(last_sunday(2027,3)==28 && last_sunday(2027,10)==31);
static_assert(last_sunday(2024,3)==31 && last_sunday(2024,10)==27);
static_assert(leap(2000) && leap(2024) && !leap(2100));
static_assert(valid({2024,2,29,23,59,59}) && !valid({2026,2,29,0,0,0}));
static_assert(!valid({2026,13,1,0,0,0}) && !valid({2026,1,1,24,0,0}));
static_assert(!valid({2026,1,1,0,60,0}) && !valid({2026,1,1,0,0,60}));

void utc_case(DateTime utc, DateTime expected, int offset) {
    ZonedTime result{};
    assert(utc_to_local(utc,result));
    assert(equal(result.time,expected));
    assert(result.utc_offset_hours==offset);
    ZonedTime repeated{};
    assert(utc_to_local(utc,repeated));
    assert(equal(result.time,repeated.time));
}
int main() {
    utc_case({2026,3,29,0,59,59},{2026,3,29,1,59,59},1);
    utc_case({2026,3,29,1,0,0},{2026,3,29,3,0,0},2);
    utc_case({2026,10,25,0,59,59},{2026,10,25,2,59,59},2);
    utc_case({2026,10,25,1,0,0},{2026,10,25,2,0,0},1);
    utc_case({2027,3,28,0,59,59},{2027,3,28,1,59,59},1);
    utc_case({2027,3,28,1,0,0},{2027,3,28,3,0,0},2);
    utc_case({2027,10,31,0,59,59},{2027,10,31,2,59,59},2);
    utc_case({2027,10,31,1,0,0},{2027,10,31,2,0,0},1);
    utc_case({2026,12,31,23,30,0},{2027,1,1,0,30,0},1);
    utc_case({2024,2,29,22,30,0},{2024,2,29,23,30,0},1);
    utc_case({2026,3,31,22,30,0},{2026,4,1,0,30,0},2);

    auto gap=local_to_utc({2026,3,29,2,30,0});
    assert(gap.status==LocalStatus::Nonexistent);
    assert(local_to_utc({2027,3,28,2,0,0}).status==LocalStatus::Nonexistent);
    auto overlap=local_to_utc({2026,10,25,2,30,0});
    assert(overlap.status==LocalStatus::Ambiguous);
    assert(equal(overlap.daylight_utc,{2026,10,25,0,30,0}));
    assert(equal(overlap.standard_utc,{2026,10,25,1,30,0}));
    assert(local_to_utc({2027,10,31,2,30,0}).status==LocalStatus::Ambiguous);
    auto normal=local_to_utc({2026,1,1,12,30,0});
    assert(normal.status==LocalStatus::Unique);
    assert(equal(normal.standard_utc,{2026,1,1,11,30,0}));
    normal=local_to_utc({2026,7,1,12,30,0});
    assert(normal.status==LocalStatus::Unique);
    assert(equal(normal.daylight_utc,{2026,7,1,10,30,0}));
    assert(local_to_utc({2026,2,30,0,0,0}).status==LocalStatus::Invalid);
    ZonedTime invalid{};
    assert(!utc_to_local({2026,2,30,0,0,0},invalid));
    assert(local_to_utc({2000,1,1,0,30,0}).status==LocalStatus::Nonexistent);
    assert(!utc_to_local({2099,12,31,23,59,0},invalid));
    for(unsigned m=1;m<=12;++m) {
        const DateTime utc{2026,static_cast<std::uint8_t>(m),15,12,0,0};
        ZonedTime first{},second{};
        assert(utc_to_local(utc,first) && utc_to_local(utc,second));
        assert(equal(first.time,second.time));
        const auto reverse=local_to_utc(first.time);
        assert(reverse.status==LocalStatus::Unique);
        assert(equal(first.utc_offset_hours==1?reverse.standard_utc:reverse.daylight_utc,utc));
    }
}
