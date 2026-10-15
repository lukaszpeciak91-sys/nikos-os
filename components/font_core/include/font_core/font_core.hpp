#pragma once
#include <cstddef>
#include <cstdint>
#include "board/board.hpp"

namespace nikos::font_core {

enum class Mode : std::uint8_t { Standard, Composed, PreRendered };

struct Metrics {
    std::uint8_t line_height;
    std::uint8_t glyph_height;
    std::uint8_t letter_spacing;
};

struct Result {
    bool fits = false;
    bool supported = true;
    std::uint16_t width_px = 0;
    std::uint16_t height_px = 0;
    std::uint8_t lines = 0;
};

struct Region {
    std::int16_t x;
    std::int16_t y;
    std::int16_t width;
    std::int16_t height;
};

// A per-client, boot-scoped renderer. Never alters other Board text calls.
// Explicit fit result: no pixels drawn on failure.
class Renderer final {
public:
    explicit Renderer(board::Board& board) : board_(board) {}
    void select(Mode mode) { mode_ = mode; }
    Mode mode() const { return mode_; }
    Metrics metrics() const;
    std::uint8_t advance(std::uint32_t codepoint) const;
    Result measure(const char* utf8, std::int16_t available_width,
                   std::int16_t available_height) const;
    Result draw(const char* utf8, Region region,
                board::DisplayColor foreground = board::DisplayColor::PrimaryText,
                board::DisplayColor background = board::DisplayColor::Background);
private:
    board::Board& board_;
    Mode mode_ = Mode::Standard;
};
}
