#include "facts/quiz.hpp"
#include "facts/quiz_data.hpp"
#include <cstdio>
#include <cstring>
#include "esp_random.h"
namespace nikos::facts {
namespace {
void selection(nikos::board::Board& b, int x, int y) {
    b.draw_line(x, y, x, y + 15, board::DisplayColor::Accent);
    b.draw_line(x + 1, y, x + 1, y + 15, board::DisplayColor::Accent);
}
}
void Quiz::start() {
    std::uint8_t deck[quiz::kItemCount]{};
    for (std::uint8_t i = 0; i < quiz::kItemCount; ++i) deck[i] = i;
    for (std::uint8_t i = 0; i < 10; ++i) {
        const std::uint8_t pick = static_cast<std::uint8_t>(
            i + esp_random() % (quiz::kItemCount - i));
        const std::uint8_t chosen = deck[pick];
        deck[pick] = deck[i];
        deck[i] = chosen;
        indices_[i] = chosen;
    }
    position_ = 0;
    text_page_ = 0;
    score_ = 0;
    selected_ = 0;
    final_selected_ = 0;
    screen_ = Screen::Question;
    redraw();
}
bool Quiz::update(const board::InputState& input) {
    if (input.secondary_long) return true;
    if (screen_ == Screen::Question) {
        if (input.secondary_short) {
            selected_ ^= 1U;
            redraw();
        } else if (input.primary_short) {
            if (text_page_ + 1U < text_page_count_) {
                ++text_page_;
                redraw();
                return false;
            }
            text_page_ = 0;
            last_correct_ = (selected_ == 0U)
                == quiz::kItems[indices_[position_]].correct;
            if (last_correct_) ++score_;
            screen_ = Screen::Feedback;
            redraw();
        }
    } else if (screen_ == Screen::Feedback) {
        if (input.primary_short) {
            if (text_page_ + 1U < text_page_count_) {
                ++text_page_;
                redraw();
                return false;
            }
            text_page_ = 0;
            if (++position_ == 10U) {
                screen_ = Screen::Final;
            } else {
                selected_ = 0;
                screen_ = Screen::Question;
            }
            redraw();
        }
    } else {
        if (input.secondary_short) {
            final_selected_ ^= 1U;
            redraw();
        } else if (input.primary_short) {
            if (final_selected_ == 1U) return true;
            start();
        }
    }
    return false;
}
void Quiz::redraw() {
    switch (screen_) {
        case Screen::Question: render_question(); break;
        case Screen::Feedback: render_feedback(); break;
        case Screen::Final: render_final(); break;
    }
}
std::uint16_t Quiz::text_width(const char* text) const
{
    if (settings_.font_mode == font_core::Mode::Standard)
        return static_cast<std::uint16_t>(2 * board_.polish_ui_text_width(text));
    auto& renderer = const_cast<font_core::Renderer&>(renderer_);
    renderer.select(settings_.font_mode);
    return renderer.measure(text, 30000, 30000).width_px;
}

void Quiz::layout_text_pages(const char* text, std::uint8_t max_lines)
{
    constexpr std::uint16_t kWidth = 226;
    text_page_count_ = 0;
    const std::size_t length = std::strlen(text);
    std::size_t offset = 0;
    while (offset < length && text_page_count_ < 8) {
        while (offset < length && text[offset] == ' ') ++offset;
        if (offset == length) break;
        const std::size_t page_start = offset;
        std::size_t line_start = offset;
        std::size_t accepted = offset;
        std::uint8_t lines = 1;
        while (offset < length) {
            while (offset < length && text[offset] == ' ') ++offset;
            std::size_t word_end = offset;
            while (word_end < length && text[word_end] != ' ') ++word_end;
            if (word_end == offset) break;
            const std::size_t bytes = word_end - line_start;
            if (bytes >= sizeof(text_buffer_)) break;
            std::memcpy(text_buffer_, text + line_start, bytes);
            text_buffer_[bytes] = '\0';
            if (text_width(text_buffer_) > kWidth) {
                if (accepted == line_start) break;
                if (lines >= max_lines) break;
                ++lines;
                line_start = offset;
                const auto word_bytes = word_end - line_start;
                if (word_bytes >= sizeof(text_buffer_)) break;
                std::memcpy(text_buffer_, text + line_start, word_bytes);
                text_buffer_[word_bytes] = '\0';
                if (text_width(text_buffer_) > kWidth) break;
            }
            accepted = word_end;
            offset = word_end;
        }
        if (accepted == page_start) {
            text_page_count_ = 0;
            return;
        }
        text_ranges_[text_page_count_++] = Range{
            static_cast<std::uint16_t>(page_start),
            static_cast<std::uint16_t>(accepted)
        };
        offset = accepted;
    }
    if (offset < length) text_page_count_ = 0;
}

void Quiz::draw_text_page(const char* text, std::int16_t y,
                          std::int16_t height, std::uint8_t max_lines)
{
    layout_text_pages(text, max_lines);
    if (text_page_count_ == 0 || text_page_ >= text_page_count_) {
        board_.draw_text_region(7, y, 226, 18,
            "TEXT FIT ERROR", 1, board::DisplayColor::Danger);
        return;
    }
    const Range r = text_ranges_[text_page_];
    std::size_t out = 0;
    std::size_t line_start = r.begin;
    for (std::size_t cursor = r.begin; cursor < r.end;) {
        while (cursor < r.end && text[cursor] == ' ') ++cursor;
        if (cursor >= r.end) break;
        std::size_t word_end = cursor;
        while (word_end < r.end && text[word_end] != ' ') ++word_end;
        const std::size_t line_bytes = word_end - line_start;
        if (line_bytes >= sizeof(text_buffer_)) break;
        char line[192]{};
        std::memcpy(line, text + line_start, line_bytes);
        line[line_bytes] = '\0';
        if (text_width(line) > 226 && out > 0) {
            text_buffer_[out++] = '\n';
            line_start = cursor;
        } else if (out > 0) {
            text_buffer_[out++] = ' ';
        }
        if (out + word_end - cursor >= sizeof(text_buffer_)) break;
        std::memcpy(text_buffer_ + out, text + cursor, word_end - cursor);
        out += word_end - cursor;
        text_buffer_[out] = '\0';
        cursor = word_end;
    }
    if (settings_.font_mode == font_core::Mode::Standard) {
        board_.draw_polish_ui_text_region(7, y, 226, height, text_buffer_, 2);
    } else {
        renderer_.select(settings_.font_mode);
        const auto result = renderer_.draw(text_buffer_, {7, y, 226, height});
        if (!result.fits || !result.supported) {
            board_.draw_text_region(7, y, 226, 17,
                "FONT FIT ERROR", 1, board::DisplayColor::Danger);
        }
    }
}

void Quiz::draw_wrapped(const char* text, std::int16_t y,
                        std::uint8_t max_lines)
{
    draw_text_page(text, y, static_cast<std::int16_t>(max_lines * 24),
                   max_lines);
}
void Quiz::draw_selected_text(const char* text, std::int16_t x,
    std::int16_t y, std::int16_t width, std::int16_t height,
    board::DisplayColor foreground, board::DisplayColor background)
{
    if (settings_.font_mode == font_core::Mode::Standard) {
        board_.draw_polish_ui_text_region(x, y, width, height,
            text, 2, foreground, background);
    } else {
        renderer_.select(settings_.font_mode);
        const auto result = renderer_.draw(
            text, {x, y, width, height}, foreground, background);
        if (!result.fits || !result.supported)
            board_.draw_text_region(x, y, width, height,
                "FONT FIT ERROR", 1, board::DisplayColor::Danger);
    }
}
void Quiz::render_question() {
    board_.clear_screen();
    char progress[24]{};
    std::snprintf(progress, sizeof(progress), "QUIZ %02u/10",
        static_cast<unsigned>(position_ + 1U));
    board_.draw_text_region(8, 3, 130, 18, progress, 2);
    char points[16]{};
    std::snprintf(points, sizeof(points), "%u PKT", static_cast<unsigned>(score_));
    board_.draw_text_region(162, 5, 74, 13, points, 1);
    board_.draw_line(7, 23, 233, 23, board::DisplayColor::Accent);
    draw_wrapped(quiz::kItems[indices_[position_]].statement, 28, 3);
    if (text_page_count_ > 1) {
        char page[12]{};
        std::snprintf(page, sizeof(page), "%u/%u M5",
            static_cast<unsigned>(text_page_ + 1U),
            static_cast<unsigned>(text_page_count_));
        board_.draw_text_region(174, 100, 62, 9, page, 1,
            board::DisplayColor::Accent);
    }
    draw_selected_text("PRAWDA", 18, 109, 92, 25,
        selected_ == 0 ? board::DisplayColor::PrimaryText : board::DisplayColor::SecondaryText,
        selected_ == 0 ? board::DisplayColor::Surface : board::DisplayColor::Background);
    draw_selected_text("FAŁSZ", 144, 109, 92, 25,
        selected_ == 1 ? board::DisplayColor::PrimaryText : board::DisplayColor::SecondaryText,
        selected_ == 1 ? board::DisplayColor::Surface : board::DisplayColor::Background);
    selection(board_, selected_ == 0 ? 10 : 136, 112);
}
void Quiz::render_feedback() {
    board_.clear_screen();
    const auto& item = quiz::kItems[indices_[position_]];
    board_.draw_text_region(8, 9, 224, 22,
        last_correct_ ? "BRAWO!" : "NIE TYM RAZEM!", 2,
        last_correct_ ? board::DisplayColor::StatusActive
                      : board::DisplayColor::Attention);
    draw_selected_text(last_correct_ ? "+1 PKT"
            : (item.correct ? "POPRAWNA: PRAWDA" : "POPRAWNA: FAŁSZ"),
        8, 37, 224, 25, board::DisplayColor::PrimaryText,
        board::DisplayColor::Background);
    board_.draw_line(8, 61, 232, 61, board::DisplayColor::Accent);
    draw_wrapped(item.explanation, 65, 2);
    if (text_page_count_ > 1) {
        char page[12]{};
        std::snprintf(page, sizeof(page), "%u/%u M5",
            static_cast<unsigned>(text_page_ + 1U),
            static_cast<unsigned>(text_page_count_));
        board_.draw_text_region(174, 113, 62, 9, page, 1,
            board::DisplayColor::Accent);
    }
    board_.draw_text_region(135, 121, 103, 12, "M5 DALEJ", 1,
        board::DisplayColor::SecondaryText);
}
void Quiz::render_final() {
    board_.clear_screen();
    board_.draw_text_region(10, 9, 220, 20, "KONIEC QUIZU", 2);
    char result[16]{};
    std::snprintf(result, sizeof(result), "%u / 10",
                  static_cast<unsigned>(score_));
    board_.draw_text_region(50, 39, 150, 35, result, 4);
    board_.draw_text_region(24, 85, 192, 17, "JESZCZE RAZ", 2,
        final_selected_ == 0 ? board::DisplayColor::PrimaryText :
                              board::DisplayColor::SecondaryText);
    board_.draw_text_region(24, 108, 192, 17, "POWROT", 2,
        final_selected_ == 1 ? board::DisplayColor::PrimaryText :
                              board::DisplayColor::SecondaryText);
    selection(board_, 15, final_selected_ == 0 ? 85 : 108);
}
}
