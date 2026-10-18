#pragma once
#include "clock/warsaw_time_rules.hpp"
#include <cstdint>

namespace nikos::clock::world {
using warsaw::DateTime;
enum class City : std::uint8_t {
    Warsaw, London, Reykjavik, Cairo, CapeTown, NewYork, LosAngeles,
    Rio, BuenosAires, Dubai, Delhi, Bangkok, Tokyo, Sydney, Auckland
};
constexpr std::uint8_t kCityCount=15;
struct Reading { bool valid=false; DateTime local{}; std::int16_t offset_minutes=0; };
constexpr const char* city_name(City city) {
    switch(city) {
        case City::Warsaw: return "WARSZAWA";
        case City::London: return "LONDYN";
        case City::Reykjavik: return "REYKJAVÍK";
        case City::Cairo: return "KAIR";
        case City::CapeTown: return "KAPSZTAD";
        case City::NewYork: return "NOWY JORK";
        case City::LosAngeles: return "LOS ANGELES";
        case City::Rio: return "RIO DE JANEIRO";
        case City::BuenosAires: return "BUENOS AIRES";
        case City::Dubai: return "DUBAJ";
        case City::Delhi: return "DELHI";
        case City::Bangkok: return "BANGKOK";
        case City::Tokyo: return "TOKIO";
        case City::Sydney: return "SYDNEY";
        case City::Auckland: return "AUCKLAND";
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
// Friday=5, Thursday=4 with Sunday=0.
constexpr unsigned last_weekday(unsigned y,unsigned m,unsigned day_of_week) {
    const unsigned last=warsaw::month_days(y,static_cast<std::uint8_t>(m));
    return last-(weekday(y,m,last)+7U-day_of_week)%7U;
}
// Current Egypt rules since 2023. Transition at local midnight Friday:
// start Thursday 22:00 UTC (+02), end Thursday 21:00 UTC (+03).
constexpr bool cairo_dst(DateTime utc) {
    const auto y=utc.year;
    const auto april=last_weekday(y,4,5);
    const auto october=last_weekday(y,10,4);
    const auto start=warsaw::seconds({y,4,static_cast<std::uint8_t>(april),0,0,0})-2*3600;
    const auto end=warsaw::seconds({y,10,static_cast<std::uint8_t>(october),0,0,0})+21*3600;
    return warsaw::seconds(utc)>=start && warsaw::seconds(utc)<end;
}
// NZ rules adopted in 2007: September last Sunday 02:00 NZST and
// April first Sunday 03:00 NZDT, both at 14:00 UTC previous day.
constexpr bool auckland_dst(DateTime utc) {
    const auto y=utc.year;
    const auto end=warsaw::seconds({y,4,static_cast<std::uint8_t>(nth_sunday(y,4,1)),0,0,0})-10*3600;
    const auto start=warsaw::seconds({y,9,warsaw::last_sunday(y,9),0,0,0})-10*3600;
    const auto current=warsaw::seconds(utc);
    return current<end || current>=start;
}
constexpr Reading convert(City city,DateTime utc) {
    Reading out{};
    if (!warsaw::valid(utc)) return out;
    // Historical legal transitions were not stable. Never infer unknown eras.
    if (city==City::Sydney && utc.year<2008) return out;
    if (city==City::Auckland && utc.year<2008) return out;
    if (city==City::Cairo && utc.year<2023) return out;
    if (city==City::Rio && utc.year<2020) return out;
    if (city==City::BuenosAires && utc.year<2010) return out;
    int offset_minutes=0;
    switch(city) {
        case City::Warsaw: offset_minutes=warsaw::daylight_utc(utc)?120:60;break;
        case City::London: offset_minutes=london_dst(utc)?60:0;break;
        case City::Reykjavik: offset_minutes=0;break;
        case City::Cairo: offset_minutes=cairo_dst(utc)?180:120;break;
        case City::CapeTown: offset_minutes=120;break;
        case City::NewYork: offset_minutes=ny_dst(utc)?-240:-300;break;
        case City::LosAngeles: offset_minutes=ny_dst(utc)?-420:-480;break;
        case City::Rio: offset_minutes=-180;break;
        case City::BuenosAires: offset_minutes=-180;break;
        case City::Dubai: offset_minutes=240;break;
        case City::Delhi: offset_minutes=330;break;
        case City::Bangkok: offset_minutes=420;break;
        case City::Tokyo: offset_minutes=540;break;
        case City::Sydney: offset_minutes=sydney_dst(utc)?660:600;break;
        case City::Auckland: offset_minutes=auckland_dst(utc)?780:720;break;
        default: return out;
    }
    DateTime local{};
    if (!warsaw::from_seconds(warsaw::seconds(utc)+offset_minutes*60,local))
        return out;
    out.valid=true;
    out.local=local;
    out.offset_minutes=static_cast<std::int16_t>(offset_minutes);
    return out;
}
} // namespace nikos::clock::world
