#include "coin_flip/coin_flip_app.hpp"
#include "esp_random.h"
#include "esp_timer.h"
namespace nikos::coin_flip {
std::uint32_t CoinFlipApp::now_ms() {
    return static_cast<std::uint32_t>(esp_timer_get_time() / 1000ULL);
}
void CoinFlipApp::begin() {
    active_ = true;
    state_.begin(now_ms());
    board_.clear_screen();
    render();
}
void CoinFlipApp::resume() {
    if (!active_) return;
    state_.resume(now_ms());
    board_.clear_screen();
    render();
}
void CoinFlipApp::redraw() {
    if (!active_ || state_.suspended) return;
    board_.clear_screen();
    render();
}
CoinFlipApp::Result CoinFlipApp::update(const board::InputState& input) {
    if (!active_ || input.secondary_long) return Result::ExitRequested;
    if (state_.suspended) return Result::Running;
    const auto now = now_ms();
    const bool animation_at_entry = state_.phase == Phase::Flipping;
    if (state_.tick(now)) render();
    if (input.primary_short && !animation_at_entry) {
        // Hardware randomness is sampled only when a new toss is accepted.
        if (state_.toss(now, esp_random())) render();
    }
    return Result::Running;
}
void CoinFlipApp::draw_coin(std::uint8_t frame) {
    const std::uint8_t shape = frame % 6U;
    const std::int16_t half_width[6] = {36, 22, 3, 17, 32, 36};
    const std::int16_t half = half_width[shape];
    const std::int16_t cx = 120, cy = 70, radius = 35;
    // Face-on frames use true circle primitives. Narrow intermediate frames
    // remain lightweight ellipses so the existing flip motion is preserved.
    if (half >= 32) {
        // Keep the warm gold rim, but use the theme's dark Surface within it.
        // PrimaryText must contrast with the face in *every* OS theme.
        board_.fill_circle(cx, cy, radius, board::DisplayColor::CoinGold);
        board_.fill_circle(cx, cy, radius - 3, board::DisplayColor::Surface);
        board_.draw_line(cx - 14, cy - 25, cx + 10, cy - 25,
            board::DisplayColor::CoinGold);
    } else {
        for (std::int16_t y = -radius; y <= radius; ++y) {
            const std::int32_t distance = radius * radius - y * y;
            std::int16_t x = 0;
            while ((x + 1) * (x + 1) * radius * radius
                    <= half * half * distance) ++x;
            board_.draw_line(cx - x, cy + y, cx + x, cy + y,
                board::DisplayColor::CoinGold);
        }
    }
    if (half < 28) return;
    const bool eagle = state_.phase == Phase::Idle
        || ((state_.phase == Phase::Flipping && (frame / 3U) % 2U == 0U)
            || (state_.phase == Phase::Result && state_.result == Face::Eagle));
    if (eagle) {
        // Two-pixel outer wing contours; leave the central body at its
        // original 10 px width and the crown at 4 px.
        board_.draw_line(120, 61, 101, 52, board::DisplayColor::PrimaryText);
        board_.draw_line(120, 62, 101, 53, board::DisplayColor::PrimaryText);
        board_.draw_line(120, 61, 139, 52, board::DisplayColor::PrimaryText);
        board_.draw_line(120, 62, 139, 53, board::DisplayColor::PrimaryText);
        board_.draw_line(101, 52, 108, 70, board::DisplayColor::PrimaryText);
        board_.draw_line(102, 52, 109, 70, board::DisplayColor::PrimaryText);
        board_.draw_line(139, 52, 132, 70, board::DisplayColor::PrimaryText);
        board_.draw_line(138, 52, 131, 70, board::DisplayColor::PrimaryText);
        board_.fill_rect(115, 59, 10, 17, board::DisplayColor::PrimaryText);
        board_.draw_line(120, 76, 113, 86, board::DisplayColor::PrimaryText);
        board_.draw_line(121, 76, 114, 86, board::DisplayColor::PrimaryText);
        board_.draw_line(120, 76, 127, 86, board::DisplayColor::PrimaryText);
        board_.draw_line(119, 76, 126, 86, board::DisplayColor::PrimaryText);
        board_.fill_rect(118, 54, 4, 5, board::DisplayColor::PrimaryText);
    } else {
        // Font0 is 6x8 px at scale 1; keep the existing 3x numeral style.
        // Its 18x24 text cell is centered around the coin, not the screen.
        constexpr std::int16_t digit_width = 6 * 3;
        constexpr std::int16_t digit_height = 8 * 3;
        board_.draw_text_region(
            cx - digit_width / 2, cy - 22,
            digit_width, digit_height, "1", 3,
            board::DisplayColor::PrimaryText, board::DisplayColor::Surface);

        font_.select(font_core::Mode::PreRendered);
        font_.set_size(font_core::Size::Compact);
        constexpr const char* label = "ZŁOTY";
        const auto measured = font_.measure(label, 2 * radius,
            font_.metrics().line_height);
        if (measured.fits && measured.supported) {
            // The glyph raster has a 1 px left ink inset and 2 px right;
            // +1 makes its actual pixels center at cx (not just its advance).
            const std::int16_t label_x = static_cast<std::int16_t>(
                cx - measured.width_px / 2 + 1);
            font_.draw(label, {label_x, cy + 2,
                static_cast<std::int16_t>(measured.width_px),
                static_cast<std::int16_t>(measured.height_px)},
                board::DisplayColor::PrimaryText, board::DisplayColor::Surface);
        }
    }
}
void CoinFlipApp::render() {
    // Redraw only the coin's bounded canvas; no full-display flash per frame.
    board_.fill_rect(76, 31, 88, 79, board::DisplayColor::Background);
    if (state_.phase == Phase::Idle || state_.phase == Phase::Result) {
        draw_coin(state_.phase == Phase::Idle ? 0 : 5);
    } else {
        draw_coin(state_.frame);
    }
    const char* heading = state_.phase == Phase::Result
        ? (state_.result == Face::Eagle ? "ORZEŁ!" : "RESZKA!")
        : "ORZEŁ / RESZKA";
    font_.select(font_core::Mode::PreRendered);
    font_.set_size(font_core::Size::Compact);
    font_.draw(heading, {12, 5, 216, 19},
        board::DisplayColor::PrimaryText, board::DisplayColor::Background);
    const char* footer = state_.phase == Phase::Result ? "M5: PONOWNIE"
        : state_.phase == Phase::Idle ? "M5: RZUĆ" : "";
    font_.draw(footer, {40, 114, 160, 19},
        board::DisplayColor::SecondaryText, board::DisplayColor::Background);
}
}
