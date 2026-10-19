#pragma once
#include <algorithm>
#include <array>
#include <cstdint>
#include <cstring>

namespace nikos::board {
enum class DisplayColor : std::uint8_t {
    Background, Surface, PrimaryText, SecondaryText, Accent, CoinGold,
    StatusActive, StatusInactive, Attention, Danger
};
struct InputState {
    bool primary_pressed=false, secondary_pressed=false, power_short=false;
    bool primary_short=false, primary_long=false;
    bool secondary_short=false, secondary_long=false;
};
class Board {
public:
    static constexpr int kWidth=240, kHeight=135;
    struct NativeText {
        std::int16_t x=0, y=0, w=0, h=0;
        std::uint8_t size=0;
        char text[16]{};
        bool called=false;
    };
    std::array<DisplayColor,kWidth*kHeight> pixels{};
    bool bounds_ok=true;
    NativeText native{};
    Board() { clear_screen(); }
    void clear_screen() { pixels.fill(DisplayColor::Background); native.called=false; }
    void set_pixel(int x,int y,DisplayColor color) {
        bounds_ok = bounds_ok && x>=0 && y>=0 && x<kWidth && y<kHeight;
        if(x>=0 && y>=0 && x<kWidth && y<kHeight)
            pixels[y*kWidth+x]=color;
    }
    DisplayColor at(int x,int y) const { return pixels[y*kWidth+x]; }
    void fill_rect(std::int16_t x,std::int16_t y,
        std::int16_t w,std::int16_t h,DisplayColor color) {
        bounds_ok = bounds_ok && x>=0 && y>=0 && w>0 && h>0
            && x+w<=kWidth && y+h<=kHeight;
        for(int py=y;py<y+h;py++)for(int px=x;px<x+w;px++)set_pixel(px,py,color);
    }
    void fill_circle(std::int16_t cx,std::int16_t cy,
        std::int16_t radius,DisplayColor color) {
        for(int dy=-radius;dy<=radius;dy++)
            for(int dx=-radius;dx<=radius;dx++)
                if(dx*dx+dy*dy<=radius*radius)set_pixel(cx+dx,cy+dy,color);
    }
    void draw_line(std::int16_t x0,std::int16_t y0,
        std::int16_t x1,std::int16_t y1,DisplayColor color) {
        int dx=std::abs(int(x1)-int(x0));
        int sx=x0<x1?1:-1;
        int dy=-std::abs(int(y1)-int(y0));
        int sy=y0<y1?1:-1;
        int error=dx+dy;
        while(true) {
            set_pixel(x0,y0,color);
            if(x0==x1 && y0==y1)break;
            const int twice=2*error;
            if(twice>=dy){error+=dy;x0+=sx;}
            if(twice<=dx){error+=dx;y0+=sy;}
        }
    }
    void draw_text_region(std::int16_t x,std::int16_t y,
        std::int16_t w,std::int16_t h,const char* text,
        std::uint8_t size,DisplayColor,DisplayColor bg) {
        native={};
        native.x=x; native.y=y; native.w=w; native.h=h;
        native.size=size; native.called=true;
        const auto length = std::min(std::strlen(text), sizeof(native.text)-1U);
        std::memcpy(native.text, text, length);
        native.text[length] = '\0';
        // The native Font0 raster is owned by M5GFX and not reimplemented
        // in this stub; assert its existing 6x8 cell placement instead.
        fill_rect(x,y,w,h,bg);
    }
};
} // namespace nikos::board
