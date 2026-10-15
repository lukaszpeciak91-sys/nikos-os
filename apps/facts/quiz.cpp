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
            last_correct_ = (selected_ == 0U)
                == quiz::kItems[indices_[position_]].correct;
            if (last_correct_) ++score_;
            screen_ = Screen::Feedback;
            redraw();
        }
    } else if (screen_ == Screen::Feedback) {
        if (input.primary_short) {
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
void Quiz::draw_wrapped(const char* text, std::int16_t y,
                        std::uint8_t max_lines) {
    // UTF-8 boundary-safe word wrapping, with fixed storage and no allocation.
    char line[160]{};
    std::size_t offset = 0;
    std::uint8_t line_no = 0;
    const std::size_t length = std::strlen(text);
    while (offset < length && line_no < max_lines) {
        while (offset < length && text[offset] == ' ') ++offset;
        std::size_t end = offset;
        std::size_t previous_space = offset;
        std::uint8_t cps = 0;
        while (end < length && cps < 24U) {
            if (text[end] == ' ') previous_space = end;
            const unsigned char first = static_cast<unsigned char>(text[end]);
            const std::size_t bytes = first < 0x80 ? 1U
                : (first < 0xE0 ? 2U : (first < 0xF0 ? 3U : 4U));
            if (end + bytes > length) break;
            end += bytes;
            ++cps;
        }
        if (end < length && previous_space > offset) end = previous_space;
        if (end <= offset) break;
        const std::size_t bytes = end - offset;
        if (bytes >= sizeof(line)) break;
        std::memcpy(line, text + offset, bytes);
        line[bytes] = '\0';
        board_.draw_polish_ui_text_region(7, static_cast<std::int16_t>(y + line_no * 15),
            226, 15, line, 1);
        offset = end;
        ++line_no;
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
    draw_wrapped(quiz::kItems[indices_[position_]].statement, 28, 5);
    board_.draw_polish_ui_text_region(18, 111, 92, 20, "PRAWDA", 1,
        selected_ == 0 ? board::DisplayColor::PrimaryText : board::DisplayColor::SecondaryText,
        selected_ == 0 ? board::DisplayColor::Surface : board::DisplayColor::Background);
    board_.draw_polish_ui_text_region(144, 111, 92, 20, "FAŁSZ", 1,
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
    board_.draw_polish_ui_text_region(8, 37, 224, 18,
        last_correct_ ? "+1 PKT"
            : (item.correct ? "POPRAWNA: PRAWDA" : "POPRAWNA: FAŁSZ"), 1);
    board_.draw_line(8, 61, 232, 61, board::DisplayColor::Accent);
    draw_wrapped(item.explanation, 65, 4);
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
