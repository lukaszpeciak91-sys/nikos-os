#include "coin_flip/coin_flip_app.hpp"
#include "font_core/font_core.hpp"
#include "ui_theme/ui_theme.hpp"

#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstdint>
#include <cstdio>

using nikos::board::Board;
using nikos::board::DisplayColor;
using nikos::coin_flip::CoinFlipApp;

static std::int64_t elapsed_us=0;
static std::uint32_t random_word=0;
extern "C" std::int64_t esp_timer_get_time(void) { return elapsed_us; }
extern "C" std::uint32_t esp_random(void) { return random_word; }

static nikos::board::InputState toss_button() {
    nikos::board::InputState input{};
    input.primary_short=true;
    return input;
}

static double linear(std::uint8_t channel) {
    const double v=double(channel)/255.0;
    return v<=0.04045 ? v/12.92 : std::pow((v+0.055)/1.055,2.4);
}
static double luminance(std::uint16_t rgb565) {
    const auto r=std::uint8_t((rgb565>>11)&31);
    const auto g=std::uint8_t((rgb565>>5)&63);
    const auto b=std::uint8_t(rgb565&31);
    return 0.2126*linear((r<<3)|(r>>2))
         + 0.7152*linear((g<<2)|(g>>4))
         + 0.0722*linear((b<<3)|(b>>2));
}
static double contrast(std::uint16_t a,std::uint16_t b) {
    const auto x=luminance(a),y=luminance(b);
    return (std::max(x,y)+0.05)/(std::min(x,y)+0.05);
}
static void contrast_for_all_themes() {
    using nikos::ui_theme::Theme;
    constexpr Theme themes[]{Theme::Nikos,Theme::Bursztyn,Theme::Matrix,
        Theme::Lava,Theme::Noir};
    for (auto theme:themes) {
        const auto& p=nikos::ui_theme::palette(theme);
        // Board::resolve_display_color maps the old golden face to
        // kCoinGoldRgb565 (except Noir, which substitutes accent).
        const auto before=contrast(p.primary_text,
            theme==Theme::Noir?p.accent:std::uint16_t(0xFDC8));
        const auto after=contrast(p.primary_text,p.surface);
        assert(after>=7.0 && after>before*2.0);
        std::printf("theme=%d prior=%.2f new=%.2f\n",
            int(theme),before,after);
    }
}
static void coin_bounds_and_eagle(const Board& board) {
    assert(board.bounds_ok);
    int gold=0,primary=0;
    for(int y=35;y<=105;y++)for(int x=85;x<=155;x++){
        const auto c=board.at(x,y);
        const int d=(x-120)*(x-120)+(y-70)*(y-70);
        if (c==DisplayColor::CoinGold || c==DisplayColor::PrimaryText) {
            assert(d<=35*35);
            if(c==DisplayColor::CoinGold)gold++;
            else primary++;
        }
    }
    assert(gold>400 && gold<1000); // Thin warm rim, not a yellow disk.
    assert(primary>250);           // Visible silhouette, not a few threads.
    assert(board.at(120,35)==DisplayColor::CoinGold);
    assert(board.at(120,40)==DisplayColor::Surface);
    assert(board.at(120,65)==DisplayColor::PrimaryText);
    assert(board.at(101,52)==DisplayColor::PrimaryText);
    assert(board.at(101,53)==DisplayColor::PrimaryText);
    assert(board.at(139,52)==DisplayColor::PrimaryText);
    assert(board.at(139,53)==DisplayColor::PrimaryText);
    assert(board.at(108,70)==DisplayColor::PrimaryText);
    assert(board.at(132,70)==DisplayColor::PrimaryText);
    assert(board.at(113,86)==DisplayColor::PrimaryText);
    assert(board.at(127,86)==DisplayColor::PrimaryText);
    std::printf("Eagle: gold=%d eagle=%d\n",gold,primary);
}

static void inspect_tails(Board& board) {
    assert(board.bounds_ok);
    // The numeric "1" stays native Font0 3x, nominal glyph cell 18x24.
    // Pixel-perfect native-font ink extent requires the real LCD/M5GFX.
    assert(board.native.called && board.native.size==3);
    assert(board.native.text[0]=='1' && board.native.text[1]=='\0');
    assert(board.native.x==111 && board.native.y==48);
    assert(board.native.w==18 && board.native.h==24);
    assert(2*board.native.x+board.native.w==240);
    assert(board.native.y>=35 && board.native.y+board.native.h<=105);

    // Pixel-render the real DejaVu glyph assets via the production font_core,
    // then calculate *ink* bounds, not just a text-region position.
    int left=240,right=-1,top=135,bottom=-1,ink=0;
    for(int y=72;y<91;y++)for(int x=85;x<=155;x++)
        if(board.at(x,y)==DisplayColor::PrimaryText){
            left=std::min(left,x);right=std::max(right,x);
            top=std::min(top,y);bottom=std::max(bottom,y);
            ink++;
        }
    assert(ink>45);
    assert(left>=90 && right<=150 && top>=72 && bottom<=90);
    assert(std::abs(left+right-240)<=1); // center ink exactly at x=120.
    for(int y=72;y<=90;y++)for(int x=85;x<=155;x++) {
        if(board.at(x,y)==DisplayColor::PrimaryText) {
            const int d=(x-120)*(x-120)+(y-70)*(y-70);
            assert(d<=32*32); // text never touches/overwrites gold rim
        }
    }
    nikos::font_core::Renderer font(board);
    font.select(nikos::font_core::Mode::PreRendered);
    font.set_size(nikos::font_core::Size::Compact);
    const auto metric=font.measure("ZŁOTY",70,19);
    assert(metric.supported && metric.fits);
    assert(metric.width_px==48 && metric.height_px==19);
    std::printf("Reverse: Font0 cell [%d,%d,%d,%d], ZLOTY measure=%u x %u, ink=[%d,%d]-[%d,%d]\n",
        board.native.x,board.native.y,board.native.w,board.native.h,
        metric.width_px,metric.height_px,left,top,right,bottom);
}
int main() {
    contrast_for_all_themes();
    Board board;
    CoinFlipApp app(board);
    elapsed_us=0;
    app.begin();
    coin_bounds_and_eagle(board);
    // The six face thicknesses and 900 ms state machine stay untouched.
    random_word=1;
    assert(app.update(toss_button())==CoinFlipApp::Result::Running);
    for (int ms=75;ms<=900;ms+=75) {
        elapsed_us=std::int64_t(ms)*1000;
        assert(app.update({})==CoinFlipApp::Result::Running);
        assert(board.bounds_ok);
    }
    inspect_tails(board);
    // Redraw must reproduce the same pixels and the numeral call.
    const auto final=board.pixels;
    app.redraw();
    assert(board.bounds_ok && board.pixels==final);
    app.end();

    elapsed_us=0;
    app.begin();
    random_word=0;
    assert(app.update(toss_button())==CoinFlipApp::Result::Running);
    for(int ms=75;ms<=900;ms+=75){
        elapsed_us=std::int64_t(ms)*1000;
        app.update({});
        assert(board.bounds_ok);
    }
    coin_bounds_and_eagle(board);
    assert(app.update(toss_button())==CoinFlipApp::Result::Running);
    app.end();
    std::puts("Coin Flip pixel geometry/contrast/layout/regression PASS");
}
