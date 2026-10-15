#include "font_lab/font_lab_app.hpp"
#include "font_lab/bitmap_assets.hpp"
#include <cstddef>
#include <cstdint>
namespace nikos::font_lab {
namespace {
// Three independent candidates; U8g2 is a documented research-only rejection.
constexpr const char* kNames[] = {"BITMAP GLYPH", "COMPOSED", "PRE-RENDERED"};
constexpr std::uint8_t kCandidateCount = 3;
constexpr std::uint8_t kPageCount = 6;

// A bounded UTF-8 decoder used ONLY by this diagnostic. No dependency on
// the existing GFXfont / EncodeRange / Polish UI text mapper.
std::uint32_t next_codepoint(const char*& text) {
    const unsigned char c = static_cast<unsigned char>(*text++);
    if (c < 0x80) return c;
    const std::uint32_t lead = c & (c < 0xE0 ? 0x1F : 0x0F);
    if ((c & 0xE0) == 0xC0 && (*text & 0xC0) == 0x80)
        return (lead << 6) | (static_cast<unsigned char>(*text++) & 0x3F);
    if ((c & 0xF0) == 0xE0) {
        const std::uint32_t a = static_cast<unsigned char>(*text++);
        const std::uint32_t b = static_cast<unsigned char>(*text++);
        if ((a & 0xC0) == 0x80 && (b & 0xC0) == 0x80)
            return ((c & 0x0F) << 12) | ((a & 0x3F) << 6) | (b & 0x3F);
    }
    return '?';
}
struct PolishGlyph {
    std::uint32_t codepoint;
    char ascii;
    // 5 x 8 independent fixed dot patterns (bit 4 = leftmost).
    std::uint8_t rows[8];
    std::uint8_t mark; // 1 ogonek; 2 acute; 3 dot; 4 bar
};
// Explicit outlines are separate from the system's DejaVu glyph bitmaps.
// Lowercase and uppercase are both part of the test.
constexpr PolishGlyph kGlyphs[] = {
    {0x0104,'A',{0x04,0x0E,0x11,0x11,0x1F,0x11,0x11,0x01},1},
    {0x0106,'C',{0x02,0x0E,0x11,0x10,0x10,0x11,0x0E,0x00},2},
    {0x0118,'E',{0x1F,0x10,0x10,0x1E,0x10,0x10,0x1F,0x01},1},
    {0x0141,'L',{0x10,0x10,0x14,0x18,0x10,0x10,0x1F,0x00},4},
    {0x0143,'N',{0x02,0x11,0x19,0x15,0x13,0x11,0x11,0x00},2},
    {0x00D3,'O',{0x02,0x0E,0x11,0x11,0x11,0x11,0x0E,0x00},2},
    {0x015A,'S',{0x02,0x0F,0x10,0x0E,0x01,0x11,0x1E,0x00},2},
    {0x0179,'Z',{0x02,0x1F,0x01,0x02,0x04,0x08,0x1F,0x00},2},
    {0x017B,'Z',{0x04,0x1F,0x01,0x02,0x04,0x08,0x1F,0x00},3},
    {0x0105,'a',{0,0,0x0E,0x01,0x0F,0x11,0x0F,0x01},1},
    {0x0107,'c',{0x02,0,0x0E,0x10,0x10,0x10,0x0E,0},2},
    {0x0119,'e',{0,0,0x0E,0x11,0x1F,0x10,0x0E,0x01},1},
    {0x0142,'l',{0x08,0x08,0x0C,0x0A,0x08,0x08,0x0C,0},4},
    {0x0144,'n',{0x02,0,0x1E,0x11,0x11,0x11,0x11,0},2},
    {0x00F3,'o',{0x02,0,0x0E,0x11,0x11,0x11,0x0E,0},2},
    {0x015B,'s',{0x02,0,0x0F,0x10,0x0E,0x01,0x1E,0},2},
    {0x017A,'z',{0x02,0,0x1F,0x02,0x04,0x08,0x1F,0},2},
    {0x017C,'z',{0x04,0,0x1F,0x02,0x04,0x08,0x1F,0},3},
};
const PolishGlyph* lookup(std::uint32_t cp) {
    for (const auto& glyph : kGlyphs)
        if (glyph.codepoint == cp) return &glyph;
    return nullptr;
}
char latin_base(std::uint32_t cp) {
    const PolishGlyph* p = lookup(cp);
    return p ? p->ascii : (cp >= 32 && cp <= 126 ? static_cast<char>(cp) : '?');
}
std::uint8_t b64_value(char c) {
    if (c >= 'A' && c <= 'Z') return static_cast<std::uint8_t>(c - 'A');
    if (c >= 'a' && c <= 'z') return static_cast<std::uint8_t>(c - 'a' + 26);
    if (c >= '0' && c <= '9') return static_cast<std::uint8_t>(c - '0' + 52);
    return c == '+' ? 62 : (c == '/' ? 63 : 0);
}
std::uint8_t raster_byte(const char* str, std::size_t byte_index) {
    const std::size_t group = byte_index / 3 * 4;
    const std::uint32_t combined =
        (static_cast<std::uint32_t>(b64_value(str[group])) << 18)
        | (static_cast<std::uint32_t>(b64_value(str[group+1])) << 12)
        | (static_cast<std::uint32_t>(b64_value(str[group+2])) << 6)
        | b64_value(str[group+3]);
    return static_cast<std::uint8_t>((combined >> (16 - 8 * (byte_index % 3))) & 0xFF);
}
}
void FontLabApp::begin() {
    active_ = true;
    choice_ = 0;
    page_ = 0;
    view_ = View::List;
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
    for (std::uint8_t i=0; i<kCandidateCount; ++i) {
        const std::int16_t y = 35 + 25 * i;
        const bool chosen = choice_ == i;
        board_.draw_text_region(24, y, 205, 21, kNames[i], 2,
            chosen ? board::DisplayColor::PrimaryText : board::DisplayColor::SecondaryText,
            chosen ? board::DisplayColor::Surface : board::DisplayColor::Background);
        if (chosen) board_.fill_rect(13, y, 2, 19, board::DisplayColor::Accent);
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
    switch (choice_) {
        case 0: render_direct(kTestText[page_]); break;
        case 1: render_composed(kTestText[page_]); break;
        default: render_raster(); break;
    }
    board_.draw_text_region(7, 119, 232, 12, "M5 DALEJ | SIDE LISTA", 1);
}
void FontLabApp::render_direct(const char* text) {
    // Native ASCII, but Polish letters are drawn as independent bitmaps,
    // without ever invoking the existing global Unicode mapping.
    int x = 7, y = 62;
    while (*text) {
        const auto cp = next_codepoint(text);
        if (x + 11 >= 236) { x = 7; y += 20; }
        if (const auto* glyph = lookup(cp)) {
            for (int row=0; row<8; ++row) {
                for (int col=0; col<5; ++col)
                    if (glyph->rows[row] & (1 << (4-col)))
                        board_.fill_rect(x+col*2, y+row*2, 2, 2,
                            board::DisplayColor::PrimaryText);
            }
        } else {
            char ascii[2]{latin_base(cp),0};
            board_.draw_text_region(x, y+3, 11, 16, ascii, 2);
        }
        x += 11;
    }
}
void FontLabApp::render_composed(const char* text) {
    int x = 7, y = 62;
    while (*text) {
        const auto cp = next_codepoint(text);
        if (x + 11 >= 236) { x = 7; y += 20; }
        const auto* p = lookup(cp);
        char ascii[2]{latin_base(cp),0};
        board_.draw_text_region(x, y+3, 11, 16, ascii, 2);
        if (p) {
            if (p->mark == 2) board_.draw_line(x+6,y+1,x+9,y-2,board::DisplayColor::Accent);
            if (p->mark == 3) board_.fill_rect(x+6,y,2,2,board::DisplayColor::Accent);
            if (p->mark == 1) board_.draw_line(x+8,y+17,x+6,y+19,board::DisplayColor::Accent);
            if (p->mark == 4) board_.draw_line(x+1,y+12,x+9,y+7,board::DisplayColor::Accent);
        }
        x += 11;
    }
}
void FontLabApp::render_raster() {
    // Pre-rendered entire sentences; no runtime Unicode decoder or glyph lookup.
    const char* encoded = kPreRenderedLines[page_];
    for (std::int16_t y=0; y<18; ++y) {
        for (std::int16_t x=0; x<160; ++x) {
            if (raster_byte(encoded, static_cast<std::size_t>(y*20+x/8))
                & (0x80 >> (x & 7))) {
                board_.fill_rect(11+x, 67+y*2, 1, 2,
                    board::DisplayColor::PrimaryText);
            }
        }
    }
}
}
