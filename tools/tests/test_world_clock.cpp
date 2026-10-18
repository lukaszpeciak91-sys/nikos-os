#include "clock/world_clock_rules.hpp"
#include <cassert>
using namespace nikos::clock::world;
using namespace nikos::clock::warsaw;
constexpr bool same(DateTime a,DateTime b){
    return a.year==b.year&&a.month==b.month&&a.day==b.day
        &&a.hour==b.hour&&a.minute==b.minute&&a.second==b.second;
}
void check(City city,DateTime utc,DateTime expected,int offset) {
    const auto result=convert(city,utc);
    assert(result.valid && same(result.local,expected));
    assert(result.offset_minutes==offset*60);
    assert(same(convert(city,utc).local,expected));
}
int main() {
    check(City::Warsaw,{2026,3,29,0,59,0},{2026,3,29,1,59,0},1);
    check(City::Warsaw,{2026,3,29,1,0,0},{2026,3,29,3,0,0},2);
    check(City::London,{2026,3,29,0,59,0},{2026,3,29,0,59,0},0);
    check(City::London,{2026,3,29,1,0,0},{2026,3,29,2,0,0},1);
    check(City::Warsaw,{2026,10,25,0,59,0},{2026,10,25,2,59,0},2);
    check(City::Warsaw,{2026,10,25,1,0,0},{2026,10,25,2,0,0},1);
    check(City::London,{2026,10,25,0,59,0},{2026,10,25,1,59,0},1);
    check(City::London,{2026,10,25,1,0,0},{2026,10,25,1,0,0},0);
    check(City::NewYork,{2026,3,8,6,59,0},{2026,3,8,1,59,0},-5);
    check(City::NewYork,{2026,3,8,7,0,0},{2026,3,8,3,0,0},-4);
    check(City::NewYork,{2026,11,1,5,59,0},{2026,11,1,1,59,0},-4);
    check(City::NewYork,{2026,11,1,6,0,0},{2026,11,1,1,0,0},-5);
    // Different US/European DST transition weekends.
    check(City::Warsaw,{2026,3,15,12,0,0},{2026,3,15,13,0,0},1);
    check(City::NewYork,{2026,3,15,12,0,0},{2026,3,15,8,0,0},-4);
    check(City::NewYork,{2006,4,2,6,59,0},{2006,4,2,1,59,0},-5);
    check(City::NewYork,{2006,4,2,7,0,0},{2006,4,2,3,0,0},-4);
    // Sydney first Sunday October/April (16:00 UTC previous day).
    check(City::Sydney,{2026,10,3,15,59,0},{2026,10,4,1,59,0},10);
    check(City::Sydney,{2026,10,3,16,0,0},{2026,10,4,3,0,0},11);
    check(City::Sydney,{2026,4,4,15,59,0},{2026,4,5,2,59,0},11);
    check(City::Sydney,{2026,4,4,16,0,0},{2026,4,5,2,0,0},10);
    check(City::Sydney,{2027,1,1,0,0,0},{2027,1,1,11,0,0},11);
    check(City::Tokyo,{2026,12,31,23,30,0},{2027,1,1,8,30,0},9);
    check(City::Tokyo,{2024,2,29,23,59,0},{2024,3,1,8,59,0},9);
    check(City::NewYork,{2027,1,1,0,30,0},{2026,12,31,19,30,0},-5);
    // Fixed fixtures independently checked against Python zoneinfo/IANA data.
    check(City::Reykjavik,{2026,10,18,12,0,0},{2026,10,18,12,0,0},0);
    check(City::Cairo,{2026,4,23,21,59,0},{2026,4,23,23,59,0},2);
    check(City::Cairo,{2026,4,23,22,0,0},{2026,4,24,1,0,0},3);
    check(City::Cairo,{2026,10,29,20,59,0},{2026,10,29,23,59,0},3);
    check(City::Cairo,{2026,10,29,21,0,0},{2026,10,29,23,0,0},2);
    check(City::CapeTown,{2026,12,31,23,30,0},{2027,1,1,1,30,0},2);
    check(City::LosAngeles,{2026,3,8,9,59,0},{2026,3,8,1,59,0},-8);
    check(City::LosAngeles,{2026,3,8,10,0,0},{2026,3,8,3,0,0},-7);
    check(City::LosAngeles,{2026,11,1,8,59,0},{2026,11,1,1,59,0},-7);
    check(City::LosAngeles,{2026,11,1,9,0,0},{2026,11,1,1,0,0},-8);
    check(City::Rio,{2026,7,1,12,0,0},{2026,7,1,9,0,0},-3);
    check(City::BuenosAires,{2026,7,1,12,0,0},{2026,7,1,9,0,0},-3);
    check(City::Dubai,{2026,12,31,22,30,0},{2027,1,1,2,30,0},4);
    check(City::Bangkok,{2026,12,31,22,30,0},{2027,1,1,5,30,0},7);
    check(City::Auckland,{2026,9,26,13,59,0},{2026,9,27,1,59,0},12);
    check(City::Auckland,{2026,9,26,14,0,0},{2026,9,27,3,0,0},13);
    check(City::Auckland,{2026,4,4,13,59,0},{2026,4,5,2,59,0},13);
    check(City::Auckland,{2026,4,4,14,0,0},{2026,4,5,2,0,0},12);
    const auto delhi=convert(City::Delhi,{2026,12,31,21,0,0});
    assert(delhi.valid && delhi.offset_minutes==330
        && same(delhi.local,{2027,1,1,2,30,0}));
    for(unsigned i=0;i<kCityCount;++i) {
        const auto city=static_cast<City>(i);
        assert(city_name(city)[0]!='\\0');
        assert(convert(city,{2026,10,18,12,0,0}).valid);
        assert((i+1)%kCityCount<kCityCount);
        assert((i+kCityCount-1)%kCityCount<kCityCount);
    }
    assert(!convert(City::Cairo,{2022,7,1,12,0,0}).valid);
    assert(!convert(City::Rio,{2019,7,1,12,0,0}).valid);
    assert(!convert(City::BuenosAires,{2008,7,1,12,0,0}).valid);
    assert(!convert(City::Auckland,{2006,9,1,12,0,0}).valid);
    assert(!convert(City::Sydney,{2007,10,7,0,0,0}).valid);
    assert(!convert(City::London,{2026,2,30,0,0,0}).valid);
    assert(!convert(City::Tokyo,{2099,12,31,23,59,59}).valid);
    static_assert(nth_sunday(2026,3,2)==8);
    static_assert(nth_sunday(2026,11,1)==1);
    static_assert(nth_sunday(2026,4,1)==5);
    static_assert(nth_sunday(2026,10,1)==4);
}
