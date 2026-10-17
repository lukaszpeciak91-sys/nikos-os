#pragma once
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdio>
namespace nikos::converter {
enum class Category : std::uint8_t { Temperature, Length, Mass, Volume };
struct Unit { const char* label; double scale; double offset; std::uint8_t first; std::uint8_t second; };
inline constexpr Unit temperatures[] = {{"°C",1,273.15,1,2},{"°F",5.0/9.0,255.37222222222223,0,2},{"K",1,0,0,1}};
inline constexpr Unit lengths[] = {{"mm",.001,0,1,2},{"cm",.01,0,0,4},{"m",1,0,1,5},{"km",1000,0,2,6},{"cal",.0254,0,1,0},{"stopa",.3048,0,2,4},{"mila",1609.344,0,3,2}};
inline constexpr Unit masses[] = {{"g",.001,0,1,2},{"kg",1,0,0,3},{"oz",.028349523125,0,0,3},{"lb",.45359237,0,1,2}};
inline constexpr Unit volumes[] = {{"ml",.001,0,1,2},{"l",1,0,0,2},{"US gal",3.785411784,0,1,0}};
inline const Unit* units(Category c) {
    switch(c) { case Category::Temperature:return temperatures;case Category::Length:return lengths;
    case Category::Mass:return masses;case Category::Volume:return volumes; } return temperatures;
}
inline std::uint8_t count(Category c) {
    switch(c) {case Category::Temperature:return 3;case Category::Length:return 7;
    case Category::Mass:return 4;case Category::Volume:return 3;}return 3;
}
inline bool to_base(Category c,std::uint8_t unit,double value,double& out) {
    if(unit>=count(c)||!std::isfinite(value)||value>9999.9||value< -9999.9) return false;
    if(c!=Category::Temperature&&value<0) return false;
    const Unit& u=units(c)[unit];out=value*u.scale+u.offset;
    return std::isfinite(out) && (c!=Category::Temperature||out>=-1e-9);
}
inline bool from_base(Category c,std::uint8_t unit,double base,double& out) {
    if(unit>=count(c)||!std::isfinite(base)|| (c==Category::Temperature && base< -1e-9))return false;
    const Unit& u=units(c)[unit];out=(base-u.offset)/u.scale;return std::isfinite(out);
}
inline void format(double v,char (&buf)[24]) {
    if(!std::isfinite(v)||std::fabs(v)>=1e9){std::snprintf(buf,sizeof(buf),"POZA ZAKR.");return;}
    if(v!=0&&std::fabs(v)<.000001){std::snprintf(buf,sizeof(buf),"%.2e",v);return;}
    int precision= v!=0&&std::fabs(v)<.01 ? 6 : v!=0&&std::fabs(v)<1 ? 4 : std::fabs(v)<100 ? 2 : std::fabs(v)<10000 ? 1 : 0;
    std::snprintf(buf,sizeof(buf),"%.*f",precision,v);
    char* dot=nullptr;for(char* p=buf;*p;++p)if(*p=='.')dot=p;
    if(dot){char* end=buf;while(*end)++end;while(end>dot+1&&end[-1]=='0')--end;if(end==dot+1)--end;*end=0;}
    if(buf[0]=='-'&&buf[1]=='0'&&buf[2]==0)std::snprintf(buf,sizeof(buf),"0");
}
struct Editor {
    // Sign (temperature only), four integer digits, decimal tenths.
    char digits[6]={'0','0','2','0','0','0'};
    std::uint8_t cursor=0; bool negative=false;bool temperature=false;
    void begin(double value,bool temp) {
        temperature=temp;negative=temp&&value<0;cursor=0;
        const double magnitude=std::isfinite(value)?std::fabs(value):0.;
        const auto bounded=magnitude>=9999.9 ? 99999U
            : static_cast<std::uint32_t>(magnitude*10+.5);
        constexpr std::uint32_t divisors[]={10000,1000,100,10,1};
        for(std::size_t i=0;i<5;++i)
            digits[i]=static_cast<char>('0'+(bounded/divisors[i])%10);
    }
    std::uint8_t positions()const{return temperature?6:5;}
    void increment() {
        if(temperature&&cursor==0){negative=!negative;return;}
        const auto i=static_cast<std::uint8_t>(cursor-(temperature?1:0));
        digits[i]= digits[i]=='9'?'0':static_cast<char>(digits[i]+1);
    }
    bool next(){++cursor;return cursor>=positions();}
    double value()const {
        std::uint32_t n=0;for(char c:digits)n=n*10+static_cast<std::uint32_t>(c-'0');
        return (negative?-1.0:1.0)*static_cast<double>(n)/10.;
    }
};
}
