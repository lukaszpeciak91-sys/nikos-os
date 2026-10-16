#include "font_core/font_core.hpp"
#include "glyph_assets.hpp"
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstring>

namespace nikos::font_core {
namespace {
constexpr std::uint8_t kRows = 16;
constexpr std::uint8_t kColumns = 12;
constexpr std::uint8_t kMaxLines = 7;
constexpr std::uint8_t kRasterLineHeight = 19;
constexpr std::uint8_t kLetterGap = 1;
constexpr bool large_raster(Mode mode, Size size) {
    return mode == Mode::PreRendered && size == Size::Readable;
}
constexpr std::uint8_t scaled(std::uint8_t value) {
    return static_cast<std::uint8_t>((value * 5U + 3U) / 4U);
}

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
    const Accent translated = composed(cp);
    const bool compose_mark = mode == Mode::Composed && translated.mark != 0;
    const auto* g = glyph(compose_mark
        ? static_cast<std::uint32_t>(static_cast<std::uint8_t>(translated.base))
        : cp);
    if (!g) {
        supported = false;
        g = glyph('?');
    }
    return static_cast<std::uint8_t>(g->advance + kLetterGap);
}

// Measures before any draw. Spaces are wrapping opportunities; an overlong
// word is a reported failure, never clipped or silently truncated.
Layout prepare(Mode mode, Size size, const char* text, std::int16_t max_width,
               std::int16_t max_height) {
    Layout layout;
    if (!text || max_width <= 0 || max_height <= 0) return layout;
    const std::uint8_t height = mode == Mode::Standard ? 16 :
        large_raster(mode, size) ? 24 : kRasterLineHeight;
    const char* cursor = text;
    const char* line_begin = cursor;
    std::int32_t width = 0;
    std::int32_t last_space_width = 0;
    const char* break_point = nullptr;
    bool supported = true;
    while (true) {
        const auto item = decode(cursor);
        if (!item.valid) supported = false;
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
            width = 0;
            break_point = nullptr;
            continue;
        }
        const std::uint8_t advance = static_cast<std::uint8_t>(
            (large_raster(mode, size) ? scaled(width_of(mode, item.codepoint, supported))
                : width_of(mode, item.codepoint, supported))
            * (mode == Mode::Standard ? item.next - cursor : 1));
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
            line_begin = cursor;
            width = 0;
            break_point = nullptr;
            continue;
        }
        width += advance;
        cursor = item.next;
    }
    layout.result.height_px = static_cast<std::uint16_t>(layout.result.lines * height);
    layout.result.supported = supported;
    layout.result.fits = layout.result.height_px <= max_height;
    return layout;
}
void raster_glyph(board::Board& board, const detail::GlyphAsset& asset,
                  int x, int y, Region r, board::DisplayColor color, bool large) {
    // Draw contiguous glyph runs instead of one SPI transaction per pixel.
    // Clipping stays local to the destination region; no framebuffer.
    for (int row = 0; row < kRows; ++row) {
        const char* bits = asset.rows + row * 3;
        const std::uint16_t mask = static_cast<std::uint16_t>(
            nibble(bits[0]) * 256 + nibble(bits[1]) * 16 + nibble(bits[2]));
        for (int col = 0; col < kColumns;) {
            if (!(mask & (1U << (11 - col)))) { ++col; continue; }
            const int begin = col;
            while (col < kColumns && (mask & (1U << (11 - col)))) ++col;
            const int px = x + (large ? begin * 5 / 4 : begin);
            const int right = x + (large ? col * 5 / 4 : col);
            const int py = y + (large ? row * 5 / 4 : row);
            const int bottom = y + (large ? (row + 1) * 5 / 4 : row + 1);
            const int left_clip = std::max(px, static_cast<int>(r.x));
            const int top_clip = std::max(py, static_cast<int>(r.y));
            const int right_clip = std::min(right, static_cast<int>(r.x + r.width));
            const int bottom_clip = std::min(bottom, static_cast<int>(r.y + r.height));
            if (right_clip > left_clip && bottom_clip > top_clip)
                board.fill_rect(static_cast<std::int16_t>(left_clip),
                    static_cast<std::int16_t>(top_clip),
                    static_cast<std::int16_t>(right_clip - left_clip),
                    static_cast<std::int16_t>(bottom_clip - top_clip), color);
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
        // Lowercase l advances only four pixels. Keep the stroke inside
        // that cell; uppercase L has sufficient room for the same mark.
        for (int k = 0; k < 3; ++k) plot(1+k, 9-k);
    }
}
}
Metrics Renderer::metrics() const {
    if (mode_ == Mode::Standard) return {16, 16, 0};
    if (large_raster(mode_, size_)) return {24, 20, 1};
    return {kRasterLineHeight, kRows, kLetterGap};
}
std::uint8_t Renderer::advance(std::uint32_t cp) const {
    bool supported = true;
    const auto width = width_of(mode_, cp, supported);
    return large_raster(mode_, size_) ? scaled(width) : width;
}
Result Renderer::measure(const char* text, std::int16_t w, std::int16_t h) const {
    return prepare(mode_, size_, text, w, h).result;
}
Result Renderer::draw(const char* text, Region r, board::DisplayColor fg,
                      board::DisplayColor bg) {
    // The legacy route is retained byte-for-byte in Board; no new global font
    // state is introduced. Experimental glyph data is only used by this call.
    auto layout = prepare(mode_, size_, text, r.width, r.height);
    if (!layout.result.fits || r.x < 0 || r.y < 0
        || r.width <= 0 || r.height <= 0
        || r.x + r.width > 240 || r.y + r.height > 135) {
        layout.result.fits = false;
        return layout.result;
    }
    board_.fill_rect(r.x, r.y, r.width, r.height, bg);
    if (mode_ == Mode::Standard) {
        // Existing Font0 rasterization is unchanged. Each measured line is
        // submitted separately so wrapping cannot escape the supplied region.
        for (std::uint8_t line = 0; line < layout.result.lines; ++line) {
            const auto& span = layout.lines[line];
            const std::size_t length = static_cast<std::size_t>(span.end - span.begin);
            char bounded_line[48]{};
            if (length >= sizeof(bounded_line)) {
                layout.result.fits = false;
                return layout.result;
            }
            std::memcpy(bounded_line, span.begin, length);
            board_.draw_text_region(
                r.x, static_cast<std::int16_t>(r.y + line * metrics().line_height),
                r.width, metrics().line_height, bounded_line, 1, fg, bg);
        }
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
            const auto actual_cp =
                mode_ == Mode::Composed && composed_glyph.mark != 0
                ? static_cast<std::uint32_t>(
                    static_cast<std::uint8_t>(composed_glyph.base))
                : cp.codepoint;
            const auto* image = glyph(actual_cp);
            if (!image) image = glyph('?');
            raster_glyph(board_, *image, x, y, r, fg,
                         large_raster(mode_, size_));
            if (mode_ == Mode::Composed)
                draw_mark(board_, composed_glyph.mark, x, y, r, fg);
            x += large_raster(mode_, size_)
                ? scaled(static_cast<std::uint8_t>(image->advance + kLetterGap))
                : image->advance + kLetterGap;
        }
    }
    return layout.result;
}
}  // namespace nikos::font_core
