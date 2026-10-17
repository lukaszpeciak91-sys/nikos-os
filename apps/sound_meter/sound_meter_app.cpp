#include "sound_meter/sound_meter_app.hpp"
#include <cstdio>
#include <algorithm>
#include "esp_log.h"

namespace nikos::sound_meter {
namespace {
constexpr std::uint32_t kSampleRate = 17000;
constexpr std::uint32_t kCaptureIntervalMs = 80;
constexpr std::uint32_t kCaptureTimeoutMs = 600;
constexpr std::uint32_t kWarmupBlocks = 2;
constexpr std::uint8_t kConstantBlockLimit = 8;
constexpr const char* kTag = "SoundMeter";
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
    capture_started_ms_ = 0;
    completed_blocks_ = 0;
    accepted_requests_ = 0;
    rejected_requests_ = 0;
    consecutive_constant_blocks_ = 0;
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
    capture_started_ms_ = 0;
    completed_blocks_ = 0;
    accepted_requests_ = 0;
    rejected_requests_ = 0;
    consecutive_constant_blocks_ = 0;
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
        if (board_.microphone_capture_completed(samples_)) {
            capture_pending_ = false;
            ++completed_blocks_;
            const auto bounds = std::minmax_element(
                samples_, samples_ + sizeof(samples_) / sizeof(samples_[0]));
            const std::int16_t low = *bounds.first;
            const std::int16_t high = *bounds.second;

            // The first two buffers after Mic.begin() may contain startup
            // transients. Do not feed them to the displayed peak or level.
            if (completed_blocks_ <= kWarmupBlocks) {
                ESP_LOGI(kTag, "Mic warmup block %lu min=%d max=%d",
                    static_cast<unsigned long>(completed_blocks_), low, high);
            } else {
                if (low == high) {
                    if (consecutive_constant_blocks_ < kConstantBlockLimit)
                        ++consecutive_constant_blocks_;
                } else {
                    consecutive_constant_blocks_ = 0;
                }
                if (low == high && low == static_cast<std::int16_t>(0x5A5A)) {
                    ESP_LOGE(kTag, "Mic completed request but PCM buffer was untouched");
                    microphone_ok_ = false;
                    board_.end_microphone();
                    render(true);
                    return Result::Running;
                }
                if (consecutive_constant_blocks_ >= kConstantBlockLimit) {
                    ESP_LOGE(kTag, "Mic constant PCM: %u successive blocks (%d)",
                        consecutive_constant_blocks_, low);
                    microphone_ok_ = false;
                    board_.end_microphone();
                    render(true);
                    return Result::Running;
                }
                levels_.consume(samples_, sizeof(samples_) / sizeof(samples_[0]), now_ms);
                if (completed_blocks_ <= kWarmupBlocks + 3
                    || completed_blocks_ % 64U == 0U) {
                    ESP_LOGI(kTag,
                        "Mic PCM blocks=%lu requests=%lu rejected=%u min=%d max=%d rms=%.1f level=%u",
                        static_cast<unsigned long>(completed_blocks_),
                        static_cast<unsigned long>(accepted_requests_),
                        static_cast<unsigned>(rejected_requests_),
                        low, high, static_cast<double>(levels_.last_rms()),
                        static_cast<unsigned>(levels_.level()));
                }
                render(false);
            }
        } else if (now_ms - capture_started_ms_ >= kCaptureTimeoutMs) {
            ESP_LOGE(kTag, "Mic capture timeout (%lu ms, requests=%lu completed=%lu)",
                static_cast<unsigned long>(now_ms - capture_started_ms_),
                static_cast<unsigned long>(accepted_requests_),
                static_cast<unsigned long>(completed_blocks_));
            microphone_ok_ = false;
            capture_pending_ = false;
            board_.end_microphone();
            render(true);
        }
    } else if (last_capture_ms_ == 0
               || now_ms - last_capture_ms_ >= kCaptureIntervalMs) {
        last_capture_ms_ = now_ms;
        // Sentinel prevents a stale/untouched buffer being treated as valid
        // PCM on a failed or prematurely released capture.
        std::fill_n(samples_, sizeof(samples_) / sizeof(samples_[0]),
            static_cast<std::int16_t>(0x5A5A));
        if (!board_.record_microphone(samples_,
                sizeof(samples_) / sizeof(samples_[0]), kSampleRate)) {
            ++rejected_requests_;
            ESP_LOGW(kTag, "Mic request rejected (%u consecutive)",
                static_cast<unsigned>(rejected_requests_));
            if (rejected_requests_ >= 3) {
                microphone_ok_ = false;
                board_.end_microphone();
                render(true);
            }
        } else {
            ++accepted_requests_;
            rejected_requests_ = 0;
            capture_started_ms_ = now_ms;
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
