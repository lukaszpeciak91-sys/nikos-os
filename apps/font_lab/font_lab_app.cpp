#include "font_lab/font_lab_app.hpp"
#include "font_lab/bitmap_assets.hpp"
#include <cstdint>
namespace nikos::font_lab {
namespace {
constexpr const char* kNames[] = {"STANDARD", "COMPOSED", "PRE-RENDERED"};
constexpr std::uint8_t kCandidateCount = 3;
constexpr std::uint8_t kPageCount = 6;
}
void FontLabApp::begin() {
    active_ = true;
    choice_ = 0;
    page_ = 0;
    view_ = View::List;
    renderer_.select(font_core::Mode::Standard);
    redraw();
}
bool FontLabApp::update(const board::InputState& input) {
    if (!active_ || input.secondary_long) return true;
    if (view_ == View::List) {
        if (input.secondary_short) choice_ = (choice_ + 1) % kCandidateCount;
        if (input.primary_short) { view_ = View::Sample; page_ = 0; }
        if (input.secondary_short || input.primary_short) redraw();
    } else {
        if (input.secondary_short) { view_ = View::List; redraw(); }
        else if (input.primary_short) { page_ = (page_ + 1) % kPageCount; redraw(); }
    }
    return false;
}
void FontLabApp::redraw() {
    if (!active_) return;
    if (view_ == View::List) render_list();
    else render_sample();
}
void FontLabApp::render_list() {
    board_.clear_screen();
    board_.draw_text_region(9, 6, 224, 20, "TEST FONTU", 2);
    board_.draw_line(9, 28, 230, 28, board::DisplayColor::Accent);
    for (std::uint8_t i = 0; i < kCandidateCount; ++i) {
        const std::int16_t y = 35 + 25 * i;
        const bool selected = choice_ == i;
        board_.draw_text_region(24, y, 205, 21, kNames[i], 2,
            selected ? board::DisplayColor::PrimaryText
                     : board::DisplayColor::SecondaryText,
            selected ? board::DisplayColor::Surface
                     : board::DisplayColor::Background);
        if (selected)
            board_.fill_rect(13, y, 2, 19, board::DisplayColor::Accent);
    }
    board_.draw_text_region(8, 119, 230, 13, "M5 TEST | SIDE DALEJ", 1);
}
void FontLabApp::render_sample() {
    board_.clear_screen();
    board_.draw_text_region(7, 4, 225, 18, kNames[choice_], 2,
        board::DisplayColor::Accent);
    char page[8]{'0', '1', '/', '0', '6', '\0'};
    page[1] = static_cast<char>('1' + page_);
    board_.draw_text_region(175, 25, 55, 12, page, 1);
    board_.draw_line(7, 39, 233, 39, board::DisplayColor::SecondaryText);
    renderer_.select(static_cast<font_core::Mode>(choice_));
    constexpr font_core::Region kTextRegion{7, 48, 226, 67};
    const auto result = renderer_.draw(kTestText[page_], kTextRegion);
    if (!result.fits) {
        board_.draw_text_region(8, 88, 224, 16,
            "TEXT DOES NOT FIT", 1, board::DisplayColor::Danger);
    } else if (!result.supported) {
        board_.draw_text_region(8, 102, 224, 12,
            "UNSUPPORTED GLYPH", 1, board::DisplayColor::Attention);
    }
    board_.draw_text_region(7, 119, 232, 12, "M5 DALEJ | SIDE LISTA", 1);
}
}
