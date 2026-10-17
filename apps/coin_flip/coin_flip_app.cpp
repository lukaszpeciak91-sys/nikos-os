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
        board_.fill_circle(cx, cy, radius, board::DisplayColor::PrimaryText);
        board_.fill_circle(cx, cy, radius - 3, board::DisplayColor::Attention);
        board_.draw_line(cx - 14, cy - 25, cx + 10, cy - 25,
            board::DisplayColor::PrimaryText);
    } else {
        for (std::int16_t y = -radius; y <= radius; ++y) {
            const std::int32_t distance = radius * radius - y * y;
            std::int16_t x = 0;
            while ((x + 1) * (x + 1) * radius * radius
                    <= half * half * distance) ++x;
            board_.draw_line(cx - x, cy + y, cx + x, cy + y,
                board::DisplayColor::Attention);
        }
    }
    if (half < 28) return;
    const bool eagle = state_.phase == Phase::Idle
        || ((state_.phase == Phase::Flipping && (frame / 3U) % 2U == 0U)
            || (state_.phase == Phase::Result && state_.result == Face::Eagle));
    if (eagle) {
        // Angular wings, crown and tail: readable even at 240x135.
        board_.draw_line(120, 61, 101, 52, board::DisplayColor::PrimaryText);
        board_.draw_line(120, 61, 139, 52, board::DisplayColor::PrimaryText);
        board_.draw_line(101, 52, 108, 70, board::DisplayColor::PrimaryText);
        board_.draw_line(139, 52, 132, 70, board::DisplayColor::PrimaryText);
        board_.fill_rect(115, 59, 10, 17, board::DisplayColor::PrimaryText);
        board_.draw_line(120, 76, 113, 86, board::DisplayColor::PrimaryText);
        board_.draw_line(120, 76, 127, 86, board::DisplayColor::PrimaryText);
        board_.fill_rect(118, 54, 4, 5, board::DisplayColor::PrimaryText);
    } else {
        board_.draw_text_region(106, 53, 29, 27, "1", 3,
            board::DisplayColor::PrimaryText, board::DisplayColor::Attention);
        font_.select(font_core::Mode::PreRendered);
        font_.set_size(font_core::Size::Compact);
        font_.draw("ZŁ", {105, 79, 36, 19},
            board::DisplayColor::PrimaryText, board::DisplayColor::Attention);
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
