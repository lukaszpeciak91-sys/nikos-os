#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

#include "board/board.hpp"
#include "generated/facts_data.hpp"
#include "facts/quiz.hpp"
#include "font_core/font_core.hpp"
#include "settings/settings.hpp"

namespace nikos::facts {

class FactsApp final {
public:
    enum class UpdateResult : std::uint8_t {
        Running,
        ExitRequested,
    };

    FactsApp(board::Board& board, settings::State& settings);

    void begin();
    void end();
    void redraw();
    UpdateResult update(const board::InputState& input);

private:
    enum class View : std::uint8_t {
        MainMenu,
        TitleList,
        Reader,
        Quiz,
    };

    struct PageRange {
        std::uint16_t start = 0;
        std::uint16_t end = 0;
    };

    static constexpr std::uint8_t kMainItemCount = 9;
    static constexpr std::uint8_t kMainVisibleRows = 3;
    static constexpr std::uint8_t kTitleVisibleRows = 1;
    static constexpr std::uint8_t kTitleBackIndex = 100;
    static constexpr std::uint8_t kMaxPages = 12;
    static constexpr std::size_t kPageMaxCodepoints = 120; // legacy helper limit
    static constexpr std::size_t kBodyCodepointsPerLine = 24;
    static constexpr std::size_t kBodyMaxLines = 2;
    static constexpr std::size_t kTitleCodepointsPerLine = 22;
    static constexpr std::size_t kTitleMaxLines = 2;

    static data::Category category_for_menu_index(std::uint8_t index);
    static std::uint8_t category_number(data::Category category);
    static std::size_t utf8_codepoint_bytes(unsigned char lead);
    static std::size_t count_utf8_codepoints(
        const char* text,
        std::size_t start,
        std::size_t end);
    static std::size_t range_length(const char* text);
    static std::size_t bounded_word_split(
        const char* text,
        std::size_t start,
        std::size_t end,
        std::size_t max_codepoints);
    static bool wrap_utf8(
        const char* text,
        std::size_t start,
        std::size_t end,
        std::size_t max_codepoints_per_line,
        std::size_t max_lines,
        char* output,
        std::size_t output_size);

    void update_main_viewport();
    void update_title_viewport();
    void enter_main_menu();
    void enter_category(data::Category category);
    void open_category_fact(std::uint8_t index);
    void open_random_fact();
    void open_fact(
        data::Category category,
        std::uint8_t index,
        bool random_mode);
    void advance_reader();
    void build_pages();
    void append_paginated_range(
        std::size_t start,
        std::size_t end);

    void render_main_menu();
    void render_title_list();
    void render_reader();

    board::Board& board_;
    settings::State& settings_;
    font_core::Renderer renderer_;
    Quiz quiz_;
    std::uint8_t text_line_height() const;
    std::uint16_t text_width(const char* text) const;
    bool wrap_pixels(const char* text, std::size_t start, std::size_t end,
        std::uint16_t max_width, std::uint8_t max_lines,
        char* dest, std::size_t dest_size) const;
    void render_readable(const char* text, std::int16_t x,
        std::int16_t y, std::int16_t width, std::int16_t height);

    View view_ = View::MainMenu;
    bool active_ = false;
    bool random_mode_ = false;
    bool page_overflow_ = false;

    std::uint8_t main_selection_ = 0;
    std::uint8_t main_view_start_ = 0;

    data::Category category_ = data::Category::Space;
    std::uint8_t title_selection_ = 0;
    std::uint8_t title_view_start_ = 0;

    std::uint8_t current_fact_index_ = 0;
    const data::Fact* current_fact_ = nullptr;
    std::uint16_t previous_random_flat_ = 0xFFFFU;

    std::array<PageRange, kMaxPages> pages_{};
    std::uint8_t page_count_ = 0;
    std::uint8_t page_index_ = 0;

    // Reader wrapping buffers are fixed and shared by the single composed
    // FactsApp instance, so they live in static/BSS storage rather than the
    // constrained ESP-IDF main-task stack.
    static std::array<char, 128> wrapped_title_;
    static std::array<char, 320> wrapped_body_;
    static std::array<char, 320> measure_scratch_;
};

}  // namespace nikos::facts
