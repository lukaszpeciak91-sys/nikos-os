#include "sound_meter/sound_meter_app.hpp"
#include <cstdio>

namespace nikos::sound_meter {
namespace {
constexpr std::uint32_t kSampleRate = 17000;
constexpr std::uint32_t kCaptureIntervalMs = 80;
board::DisplayColor level_color(std::uint8_t level)
{
    if (level < 25) return board::DisplayColor::StatusActive;
    if (level < 50) return board::DisplayColor::Accent;
    if (level < 75) return board::DisplayColor::Attention;
    return board::DisplayColor::Danger;
}
const char* level_label(std::uint8_t level)
{
    if (level < 25) return "CICHO";
    if (level < 50) return "UMIARKOWANIE";
    if (level < 75) return "GŁOŚNO";
    return "ZA GŁOŚNO";
}
}

void SoundMeterApp::begin()
{
    active_ = true;
    suspended_ = false;
    capture_pending_ = false;
    last_capture_ms_ = 0;
    shown_level_ = 255;
    shown_peak_ = 255;
    levels_.reset();
    microphone_ok_ = board_.begin_microphone();
    render(true);
}

void SoundMeterApp::end()
{
    if (active_) suspend();
    active_ = false;
    suspended_ = false;
    capture_pending_ = false;
    microphone_ok_ = false;
}

void SoundMeterApp::suspend()
{
    suspended_ = true;
    if (microphone_ok_) board_.end_microphone();
    microphone_ok_ = false;
    capture_pending_ = false;
}

void SoundMeterApp::resume()
{
    if (!active_ || microphone_ok_) return;
    suspended_ = false;
    microphone_ok_ = board_.begin_microphone();
    capture_pending_ = false;
    last_capture_ms_ = 0;
    render(true);
}

void SoundMeterApp::redraw()
{
    if (active_) render(true);
}

SoundMeterApp::Result SoundMeterApp::update(
    const board::InputState& input, std::uint32_t now_ms)
{
    if (!active_ || input.secondary_short || input.secondary_long) {
        return Result::ExitRequested;
    }
    if (input.primary_short) {
        levels_.reset_peak();
        shown_peak_ = 255;
    }
    if (!microphone_ok_) return Result::Running;

    if (capture_pending_) {
        if (!board_.microphone_recording()) {
            capture_pending_ = false;
            levels_.consume(samples_, sizeof(samples_) / sizeof(samples_[0]), now_ms);
            render(false);
        }
    } else if (last_capture_ms_ == 0
               || now_ms - last_capture_ms_ >= kCaptureIntervalMs) {
        last_capture_ms_ = now_ms;
        if (!board_.record_microphone(samples_,
                sizeof(samples_) / sizeof(samples_[0]), kSampleRate)) {
            microphone_ok_ = false;
            board_.end_microphone();
            render(true);
        } else {
            capture_pending_ = true;
        }
    }
    return Result::Running;
}

void SoundMeterApp::render(bool full)
{
    if (full) {
        board_.clear_screen();
        board_.draw_polish_ui_text_region(
            12, 5, 216, 20, "MIERNIK HAŁASU", 1);
        board_.draw_line(12, 25, 228, 25, board::DisplayColor::Accent);
        board_.draw_text_region(
            12, 120, 216, 12, "M5 RESET MAX | BOCZNY POWROT",
            1, board::DisplayColor::SecondaryText);
        shown_level_ = 255;
        shown_peak_ = 255;
    }
    if (!microphone_ok_) {
        board_.fill_rect(12, 34, 216, 74, board::DisplayColor::Background);
        board_.draw_text_region(12, 48, 216, 20, "BŁĄD MIKROFONU", 2,
            board::DisplayColor::Danger);
        board_.draw_text_region(12, 77, 216, 16, "BOCZNY = POWROT", 1);
        return;
    }
    const std::uint8_t current = levels_.level();
    const std::uint8_t peak = levels_.peak();
    if (!full && current == shown_level_ && peak == shown_peak_) return;

    char value[8]{};
    char peak_value[20]{};
    std::snprintf(value, sizeof(value), "%u", static_cast<unsigned>(current));
    std::snprintf(peak_value, sizeof(peak_value), "MAX %u",
                  static_cast<unsigned>(peak));

    board_.draw_text_region(12, 32, 95, 43, value, 4, level_color(current));
    board_.draw_polish_ui_text_region(
        111, 44, 119, 24, level_label(current), 1, level_color(current));
    board_.fill_rect(12, 82, 216, 15, board::DisplayColor::Surface);
    const std::int16_t fill =
        static_cast<std::int16_t>((216U * current) / 100U);
    if (fill > 0) {
        board_.fill_rect(12, 82, fill, 15, level_color(current));
    }
    const std::int16_t peak_x =
        static_cast<std::int16_t>(12 + (215U * peak) / 100U);
    board_.draw_line(peak_x, 79, peak_x, 100, board::DisplayColor::PrimaryText);
    board_.draw_text_region(12, 103, 130, 15, peak_value, 1,
        board::DisplayColor::SecondaryText);
    board_.draw_text_region(151, 103, 77, 15, "SKALA 0-100", 1,
        board::DisplayColor::SecondaryText);
    shown_level_ = current;
    shown_peak_ = peak;
}
}
