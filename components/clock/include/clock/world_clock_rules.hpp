#pragma once
#include "clock/warsaw_time_rules.hpp"
#include <cstdint>

namespace nikos::clock::world {
using warsaw::DateTime;
enum class City : std::uint8_t { Warsaw, London, NewYork, Tokyo, Sydney };
constexpr std::uint8_t kCityCount=5;
struct Reading { bool valid=false; DateTime local{}; std::int8_t offset_hours=0; };
constexpr const char* city_name(City city) {
    switch(city) {
        case City::Warsaw: return "WARSZAWA";
        case City::London: return "LONDYN";
        case City::NewYork: return "NOWY JORK";
        case City::Tokyo: return "TOKIO";
        case City::Sydney: return "SYDNEY";
    }
    return "";
}
// Jan 1 2000 = Saturday, Sunday=0.
constexpr unsigned weekday(unsigned y,unsigned m,unsigned d) {
    return static_cast<unsigned>((6+warsaw::days_since_2000(y,m,d))%7);
}
constexpr unsigned nth_sunday(unsigned y,unsigned m,unsigned n) {
    return 1U + (7U-weekday(y,m,1))%7U + 7U*(n-1U);
}
constexpr bool in_interval(std::int64_t t,DateTime begin,DateTime end) {
    return t>=warsaw::seconds(begin)&&t<warsaw::seconds(end);
}
constexpr bool ny_dst(DateTime utc) {
    const auto y=utc.year;
    // U.S. Energy Policy Act extended DST starting in 2007.
    const unsigned spring_month=y>=2007?3:4;
    const unsigned spring_day=y>=2007?nth_sunday(y,3,2):nth_sunday(y,4,1);
    const unsigned fall_month=y>=2007?11:10;
    const unsigned fall_day=y>=2007?nth_sunday(y,11,1):warsaw::last_sunday(y,10);
    return in_interval(warsaw::seconds(utc),
        {y,static_cast<std::uint8_t>(spring_month),static_cast<std::uint8_t>(spring_day),7,0,0},
        {y,static_cast<std::uint8_t>(fall_month),static_cast<std::uint8_t>(fall_day),6,0,0});
}
constexpr bool london_dst(DateTime utc) {
    const auto y=utc.year;
    return in_interval(warsaw::seconds(utc),
        {y,3,warsaw::last_sunday(y,3),1,0,0},
        {y,10,warsaw::last_sunday(y,10),1,0,0});
}
constexpr bool sydney_dst(DateTime utc) {
    // Stable NSW rules from 2008: first Sunday October 02:00 AEST,
    // first Sunday April 03:00 AEDT. Both transition at 16:00 UTC
    // on the PREVIOUS calendar day.
    const auto t=warsaw::seconds(utc);
    const auto y=utc.year;
    const auto first_apr=nth_sunday(y,4,1);
    const auto first_oct=nth_sunday(y,10,1);
    const auto spring_utc=warsaw::seconds(
        {y,10,static_cast<std::uint8_t>(first_oct),0,0,0})-8*3600;
    const auto autumn_utc=warsaw::seconds(
        {y,4,static_cast<std::uint8_t>(first_apr),0,0,0})-8*3600;
    return t<autumn_utc || t>=spring_utc;
}
constexpr Reading convert(City city,DateTime utc) {
    Reading out{};
    if (!warsaw::valid(utc)) return out;
    // Sydney pre-2008 NSW dates had different rules; do not guess.
    if (city==City::Sydney && utc.year<2008) return out;
    int offset=0;
    switch(city) {
        case City::Warsaw: offset=warsaw::daylight_utc(utc)?2:1;break;
        case City::London: offset=london_dst(utc)?1:0;break;
        case City::NewYork: offset=ny_dst(utc)?-4:-5;break;
        case City::Tokyo: offset=9;break;
        case City::Sydney: offset=sydney_dst(utc)?11:10;break;
        default: return out;
    }
    DateTime local{};
    if (!warsaw::from_seconds(warsaw::seconds(utc)+offset*3600,local))
        return out;
    out.valid=true;
    out.local=local;
    out.offset_hours=static_cast<std::int8_t>(offset);
    return out;
}
} // namespace nikos::clock::world
