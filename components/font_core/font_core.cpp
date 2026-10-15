#include "font_core/font_core.hpp"
#include "glyph_assets.hpp"
#include <algorithm>
#include <cstddef>
#include <cstdint>

namespace nikos::font_core {
namespace {
constexpr std::uint8_t kRows = 16;
constexpr std::uint8_t kColumns = 12;
constexpr std::uint8_t kMaxLines = 7;
constexpr std::uint8_t kRasterLineHeight = 19;
constexpr std::uint8_t kLetterGap = 1;

struct Decoded { std::uint32_t codepoint; const char* next; bool valid; };

Decoded decode(const char* p) {
    const auto c = static_cast<std::uint8_t>(*p);
    if (c < 0x80) return {c, p + 1, true};
    const auto continuation = [](std::uint8_t x) { return (x & 0xC0) == 0x80; };
    if (c >= 0xC2 && c <= 0xDF) {
        const auto d = static_cast<std::uint8_t>(p[1]);
        if (d != 0 && continuation(d))
            return {static_cast<std::uint32_t>((c & 31U) << 6) | (d & 63U), p + 2, true};
    } else if (c >= 0xE0 && c <= 0xEF) {
        const auto d = static_cast<std::uint8_t>(p[1]);
        if (d == 0) return {'?', p + 1, false};
        const auto e = static_cast<std::uint8_t>(p[2]);
        if (e != 0 && continuation(d) && continuation(e)
            && !(c == 0xE0 && d < 0xA0) && !(c == 0xED && d >= 0xA0)) {
            return {static_cast<std::uint32_t>((c & 15U) << 12)
                | static_cast<std::uint32_t>((d & 63U) << 6)
                | (e & 63U), p + 3, true};
        }
    }
    return {'?', p + 1, false};
}

struct Accent {
    char base;
    std::uint8_t mark; // 1 acute, 2 dot, 3 ogonek, 4 stroke
};
Accent composed(std::uint32_t cp) {
    switch(cp) {
        case 0x0104: return {'A', 3}; case 0x0105: return {'a', 3};
        case 0x0106: return {'C', 1}; case 0x0107: return {'c', 1};
        case 0x0118: return {'E', 3}; case 0x0119: return {'e', 3};
        case 0x0141: return {'L', 4}; case 0x0142: return {'l', 4};
        case 0x0143: return {'N', 1}; case 0x0144: return {'n', 1};
        case 0x00D3: return {'O', 1}; case 0x00F3: return {'o', 1};
        case 0x015A: return {'S', 1}; case 0x015B: return {'s', 1};
        case 0x0179: return {'Z', 1}; case 0x017A: return {'z', 1};
        case 0x017B: return {'Z', 2}; case 0x017C: return {'z', 2};
        default: return {static_cast<char>(cp <= 0x7F ? cp : '?'), 0};
    }
}
const detail::GlyphAsset* glyph(std::uint32_t cp) {
    if (cp >= 32 && cp <= 126) return &detail::kGlyphAssets[cp - 32];
    for (std::uint16_t i = 95; i < detail::kGlyphCount; ++i)
        if (detail::kGlyphAssets[i].codepoint == cp) return &detail::kGlyphAssets[i];
    return nullptr;
}
std::uint8_t nibble(char c) {
    if (c >= '0' && c <= '9') return static_cast<std::uint8_t>(c - '0');
    if (c >= 'a' && c <= 'f') return static_cast<std::uint8_t>(c - 'a' + 10);
    return 0;
}
struct Line { const char* begin; const char* end; std::int16_t width; };
struct Layout {
    Line lines[kMaxLines]{};
    Result result{};
};
std::uint8_t width_of(Mode mode, std::uint32_t cp, bool& supported) {
    if (mode == Mode::Standard) {
        // Legacy Font0 uses 6 horizontal pixels per input byte at size 1.
        if (cp > 0x7E || cp < 0x20) supported = false;
        return 6;
    }
    const auto* g = glyph(mode == Mode::Composed
        ? static_cast<std::uint32_t>(
            static_cast<std::uint8_t>(composed(cp).base)) : cp);
    if (!g) {
        supported = false;
        g = glyph('?');
    }
    return static_cast<std::uint8_t>(g->advance + kLetterGap);
}

// Measures before any draw. Spaces are wrapping opportunities; an overlong
// word is a reported failure, never clipped or silently truncated.
Layout prepare(Mode mode, const char* text, std::int16_t max_width,
               std::int16_t max_height) {
    Layout layout;
    if (!text || max_width <= 0 || max_height <= 0) return layout;
    const std::uint8_t height = mode == Mode::Standard ? 16 : kRasterLineHeight;
    const char* cursor = text;
    const char* line_begin = cursor;
    const char* word_begin = cursor;
    std::int32_t width = 0;
    std::int32_t word_width = 0;
    std::int32_t last_space_width = 0;
    const char* break_point = nullptr;
    bool supported = true;
    while (true) {
        const auto item = decode(cursor);
        const bool done = *cursor == '\0';
        const bool newline = *cursor == '\n';
        const bool separator = *cursor == ' ';
        if (done || newline) {
            if (layout.result.lines >= kMaxLines) return layout;
            auto& line = layout.lines[layout.result.lines++];
            line = {line_begin, cursor, static_cast<std::int16_t>(width)};
            layout.result.width_px = std::max<std::uint16_t>(
                layout.result.width_px, static_cast<std::uint16_t>(width));
            if (done) break;
            cursor = item.next;
            line_begin = cursor;
            word_begin = cursor;
            width = word_width = 0;
            break_point = nullptr;
            continue;
        }
        const std::uint8_t advance = width_of(mode, item.codepoint, supported);
        if (separator) {
            break_point = cursor;
            last_space_width = width;
        }
        if (width + advance > max_width) {
            if (!break_point || break_point < line_begin || last_space_width == 0)
                return layout;
            if (layout.result.lines >= kMaxLines) return layout;
            auto& line = layout.lines[layout.result.lines++];
            line = {line_begin, break_point, static_cast<std::int16_t>(last_space_width)};
            layout.result.width_px = std::max<std::uint16_t>(
                layout.result.width_px, static_cast<std::uint16_t>(last_space_width));
            cursor = break_point + 1;
            while (*cursor == ' ') ++cursor;
            line_begin = word_begin = cursor;
            width = word_width = 0;
            break_point = nullptr;
            continue;
        }
        width += advance;
        if (separator) {
            word_begin = item.next;
            word_width = 0;
        } else {
            word_width += advance;
        }
        cursor = item.next;
    }
    layout.result.height_px = static_cast<std::uint16_t>(layout.result.lines * height);
    layout.result.supported = supported;
    layout.result.fits = layout.result.height_px <= max_height;
    return layout;
}
void raster_glyph(board::Board& board, const detail::GlyphAsset& asset,
                  int x, int y, Region r, board::DisplayColor color) {
    for (int row = 0; row < kRows; ++row) {
        const char* bits = asset.rows + row * 3;
        const std::uint16_t mask = static_cast<std::uint16_t>(
            nibble(bits[0]) * 256 + nibble(bits[1]) * 16 + nibble(bits[2]));
        for (int col = 0; col < kColumns; ++col) {
            if (!(mask & (1U << (11 - col)))) continue;
            const int px = x + col, py = y + row;
            if (px >= r.x && py >= r.y
                && px < r.x + r.width && py < r.y + r.height)
                board.fill_rect(static_cast<std::int16_t>(px),
                                static_cast<std::int16_t>(py), 1, 1, color);
        }
    }
}
void draw_mark(board::Board& board, std::uint8_t mark, int x, int y,
               Region r, board::DisplayColor color) {
    const auto plot = [&](int dx, int dy) {
        const int px = x + dx, py = y + dy;
        if (px >= r.x && py >= r.y
            && px < r.x + r.width && py < r.y + r.height)
            board.fill_rect(static_cast<std::int16_t>(px),
                            static_cast<std::int16_t>(py), 1, 1, color);
    };
    if (mark == 1) { plot(4,1); plot(5,0); plot(6,0); }
    else if (mark == 2) { plot(5,0); plot(6,0); }
    else if (mark == 3) { plot(5,12); plot(4,13); plot(4,14); }
    else if (mark == 4) {
        for (int k = 0; k < 7; ++k) plot(1+k, 9 - k/2);
    }
}
}
Metrics Renderer::metrics() const {
    if (mode_ == Mode::Standard) return {16, 16, 0};
    return {kRasterLineHeight, kRows, kLetterGap};
}
std::uint8_t Renderer::advance(std::uint32_t cp) const {
    bool supported = true;
    return width_of(mode_, cp, supported);
}
Result Renderer::measure(const char* text, std::int16_t w, std::int16_t h) const {
    return prepare(mode_, text, w, h).result;
}
Result Renderer::draw(const char* text, Region r, board::DisplayColor fg,
                      board::DisplayColor bg) {
    // The legacy route is retained byte-for-byte in Board; no new global font
    // state is introduced. Experimental glyph data is only used by this call.
    auto layout = prepare(mode_, text, r.width, r.height);
    if (!layout.result.fits || r.x < 0 || r.y < 0
        || r.width <= 0 || r.height <= 0
        || r.x + r.width > 240 || r.y + r.height > 135) {
        layout.result.fits = false;
        return layout.result;
    }
    board_.fill_rect(r.x, r.y, r.width, r.height, bg);
    if (mode_ == Mode::Standard) {
        // Preserves existing Font0 rendering path and size.
        board_.draw_text_region(r.x, r.y, r.width, r.height, text, 1, fg, bg);
        return layout.result;
    }
    const auto m = metrics();
    for (std::uint8_t line = 0; line < layout.result.lines; ++line) {
        int x = r.x;
        const int y = r.y + line * m.line_height;
        const auto& span = layout.lines[line];
        for (const char* p = span.begin; p < span.end;) {
            const auto cp = decode(p);
            p = cp.next;
            const auto composed_glyph = composed(cp.codepoint);
            const auto actual_cp = mode_ == Mode::Composed
                ? static_cast<std::uint32_t>(
                    static_cast<std::uint8_t>(composed_glyph.base))
                : cp.codepoint;
            const auto* image = glyph(actual_cp);
            if (!image) image = glyph('?');
            raster_glyph(board_, *image, x, y, r, fg);
            if (mode_ == Mode::Composed)
                draw_mark(board_, composed_glyph.mark, x, y, r, fg);
            x += image->advance + kLetterGap;
        }
    }
    return layout.result;
}
}  // namespace nikos::font_core
