#include "facts/facts_app.hpp"

#include <algorithm>
#include <cstdio>
#include <cstring>

#include "esp_random.h"

namespace {

constexpr const char* kMainLabels[9] = {
    "LOSUJ",
    "KOSMOS",
    "ZWIERZĘTA",
    "TECHNOLOGIA",
    "CZŁOWIEK",
    "HISTORIA",
    "ŚWIAT",
    "QUIZ PRAWDA/FAŁSZ",
    "POWRÓT",
};

void draw_selection_marker(
    nikos::board::Board& board,
    std::int16_t x,
    std::int16_t y,
    std::int16_t height)
{
    const std::int16_t bottom =
        static_cast<std::int16_t>(y + height - 1);
    board.draw_line(
        x,
        y,
        x,
        bottom,
        nikos::board::DisplayColor::Accent);
    board.draw_line(
        static_cast<std::int16_t>(x + 1),
        y,
        static_cast<std::int16_t>(x + 1),
        bottom,
        nikos::board::DisplayColor::Accent);
}

}  // namespace

namespace nikos::facts {

std::array<char, 128> FactsApp::wrapped_title_{};
std::array<char, 320> FactsApp::wrapped_body_{};
std::array<char, 320> FactsApp::measure_scratch_{};

FactsApp::FactsApp(board::Board& board, settings::State& settings)
    : board_(board), settings_(settings), renderer_(board), quiz_(board, settings)
{
}

void FactsApp::begin()
{
    active_ = true;
    main_selection_ = 0;
    main_view_start_ = 0;
    enter_main_menu();
}

void FactsApp::end()
{
    active_ = false;
}

void FactsApp::redraw()
{
    if (!active_) {
        return;
    }

    switch (view_) {
        case View::Quiz:
            quiz_.redraw();
            break;
        case View::TitleList:
            render_title_list();
            break;
        case View::Reader:
            render_reader();
            break;
        case View::MainMenu:
        default:
            render_main_menu();
            break;
    }
}

FactsApp::UpdateResult FactsApp::update(
    const board::InputState& input)
{
    if (!active_) {
        return UpdateResult::ExitRequested;
    }

    if (view_ == View::Quiz) {
        if (quiz_.update(input)) enter_main_menu();
        return UpdateResult::Running;
    }

    if (input.secondary_long) {
        if (view_ == View::MainMenu) {
            return UpdateResult::ExitRequested;
        }

        enter_main_menu();
        return UpdateResult::Running;
    }

    if (view_ == View::MainMenu) {
        if (input.secondary_short) {
            main_selection_ = static_cast<std::uint8_t>(
                (main_selection_ + 1U) % kMainItemCount);
            update_main_viewport();
            render_main_menu();
            return UpdateResult::Running;
        }

        if (!input.primary_short) {
            return UpdateResult::Running;
        }

        if (main_selection_ == 0U) {
            open_random_fact();
        } else if (main_selection_ <= 6U) {
            enter_category(category_for_menu_index(main_selection_));
        } else if (main_selection_ == 7U) {
            view_ = View::Quiz;
            quiz_.start();
        } else {
            return UpdateResult::ExitRequested;
        }

        return UpdateResult::Running;
    }

    if (view_ == View::TitleList) {
        if (input.secondary_short) {
            title_selection_ = static_cast<std::uint8_t>(
                (title_selection_ + 1U)
                % (kTitleBackIndex + 1U));
            update_title_viewport();
            render_title_list();
            return UpdateResult::Running;
        }

        if (!input.primary_short) {
            return UpdateResult::Running;
        }

        if (title_selection_ == kTitleBackIndex) {
            enter_main_menu();
        } else {
            open_category_fact(title_selection_);
        }
        return UpdateResult::Running;
    }

    if (input.secondary_short) {
        if (random_mode_) {
            enter_main_menu();
        } else {
            view_ = View::TitleList;
            title_selection_ = current_fact_index_;
            update_title_viewport();
            render_title_list();
        }
        return UpdateResult::Running;
    }

    if (input.primary_short) {
        advance_reader();
    }

    return UpdateResult::Running;
}

data::Category FactsApp::category_for_menu_index(
    std::uint8_t index)
{
    switch (index) {
        case 2: return data::Category::Animals;
        case 3: return data::Category::Technology;
        case 4: return data::Category::Human;
        case 5: return data::Category::History;
        case 6: return data::Category::World;
        case 1:
        default: return data::Category::Space;
    }
}

std::uint8_t FactsApp::category_number(data::Category category)
{
    return static_cast<std::uint8_t>(category);
}

std::size_t FactsApp::utf8_codepoint_bytes(unsigned char lead)
{
    if ((lead & 0x80U) == 0U) {
        return 1U;
    }
    if ((lead & 0xE0U) == 0xC0U) {
        return 2U;
    }
    if ((lead & 0xF0U) == 0xE0U) {
        return 3U;
    }
    if ((lead & 0xF8U) == 0xF0U) {
        return 4U;
    }
    return 1U;
}

std::size_t FactsApp::count_utf8_codepoints(
    const char* text,
    std::size_t start,
    std::size_t end)
{
    std::size_t count = 0;
    for (std::size_t offset = start; offset < end;) {
        const std::size_t step =
            utf8_codepoint_bytes(
                static_cast<unsigned char>(text[offset]));
        offset += step <= end - offset ? step : 1U;
        ++count;
    }
    return count;
}

std::size_t FactsApp::range_length(const char* text)
{
    return text == nullptr ? 0U : std::strlen(text);
}

std::size_t FactsApp::bounded_word_split(
    const char* text,
    std::size_t start,
    std::size_t end,
    std::size_t max_codepoints)
{
    std::size_t offset = start;
    std::size_t codepoints = 0;
    std::size_t last_space = start;

    while (offset < end && codepoints < max_codepoints) {
        if (text[offset] == ' ') {
            last_space = offset;
        }
        const std::size_t step =
            utf8_codepoint_bytes(
                static_cast<unsigned char>(text[offset]));
        offset += step <= end - offset ? step : 1U;
        ++codepoints;
    }

    if (offset >= end) {
        return end;
    }
    if (last_space > start) {
        return last_space;
    }
    return offset;
}

bool FactsApp::wrap_utf8(
    const char* text,
    std::size_t start,
    std::size_t end,
    std::size_t max_codepoints_per_line,
    std::size_t max_lines,
    char* output,
    std::size_t output_size)
{
    if (text == nullptr
        || output == nullptr
        || output_size == 0U
        || start > end) {
        return false;
    }

    std::size_t out = 0;
    std::size_t line_count = 0;
    std::size_t cursor = start;

    while (cursor < end && line_count < max_lines) {
        while (cursor < end && text[cursor] == ' ') {
            ++cursor;
        }
        if (cursor >= end) {
            break;
        }

        std::size_t line_end = bounded_word_split(
            text,
            cursor,
            end,
            max_codepoints_per_line);
        if (line_end <= cursor) {
            return false;
        }

        std::size_t copy_end = line_end;
        while (copy_end > cursor && text[copy_end - 1U] == ' ') {
            --copy_end;
        }

        const std::size_t bytes = copy_end - cursor;
        const bool add_newline = line_count > 0U;
        if (out + bytes + (add_newline ? 1U : 0U) + 1U
            > output_size) {
            return false;
        }

        if (add_newline) {
            output[out++] = '\n';
        }
        std::memcpy(output + out, text + cursor, bytes);
        out += bytes;

        cursor = line_end;
        while (cursor < end && text[cursor] == ' ') {
            ++cursor;
        }
        ++line_count;
    }

    output[out] = '\0';
    return cursor >= end;
}

void FactsApp::update_main_viewport()
{
    if (main_selection_ < main_view_start_) {
        main_view_start_ = main_selection_;
    } else if (
        main_selection_
        >= static_cast<std::uint8_t>(
            main_view_start_ + kMainVisibleRows)) {
        main_view_start_ = static_cast<std::uint8_t>(
            main_selection_ - kMainVisibleRows + 1U);
    }

    const std::uint8_t max_start =
        kMainItemCount - kMainVisibleRows;
    if (main_view_start_ > max_start) {
        main_view_start_ = max_start;
    }
}

std::uint8_t FactsApp::title_item_height(std::uint8_t index) const
{
    const char* title = index == kTitleBackIndex ? "POWRÓT"
        : data::category_facts(category_)[index].title;
    if (!wrap_pixels(title, 0, range_length(title), 224, 3,
            wrapped_title_.data(), wrapped_title_.size())) return 80;
    std::uint8_t lines = 1;
    for (const char* p = wrapped_title_.data(); *p; ++p)
        if (*p == '\n') ++lines;
    return static_cast<std::uint8_t>(lines * text_line_height() + 4);
}

void FactsApp::update_title_viewport()
{
    if (title_selection_ < title_view_start_) title_view_start_ = title_selection_;
    while (title_view_start_ < title_selection_) {
        std::int16_t used = 0;
        for (std::uint8_t i = title_view_start_; i <= title_selection_; ++i)
            used += title_item_height(i);
        if (used <= 87) break;
        ++title_view_start_;
    }
}

void FactsApp::enter_main_menu()
{
    view_ = View::MainMenu;
    random_mode_ = false;
    update_main_viewport();
    render_main_menu();
}

void FactsApp::enter_category(data::Category category)
{
    category_ = category;
    title_selection_ = 0;
    title_view_start_ = 0;
    view_ = View::TitleList;
    render_title_list();
}

void FactsApp::open_category_fact(std::uint8_t index)
{
    open_fact(category_, index, false);
}

void FactsApp::open_random_fact()
{
    std::uint16_t flat = static_cast<std::uint16_t>(
        esp_random() % data::kTotalFacts);

    if (flat == previous_random_flat_) {
        flat = static_cast<std::uint16_t>(
            (flat + 1U) % data::kTotalFacts);
    }
    previous_random_flat_ = flat;

    const std::uint8_t category_index =
        static_cast<std::uint8_t>(
            flat / data::kFactsPerCategory);
    const std::uint8_t fact_index =
        static_cast<std::uint8_t>(
            flat % data::kFactsPerCategory);

    open_fact(
        static_cast<data::Category>(category_index),
        fact_index,
        true);
}

void FactsApp::open_fact(
    data::Category category,
    std::uint8_t index,
    bool random_mode)
{
    category_ = category;
    current_fact_index_ = index;
    random_mode_ = random_mode;
    current_fact_ =
        &data::category_facts(category_)[current_fact_index_];
    page_index_ = 0;
    build_pages();
    view_ = View::Reader;
    render_reader();
}

void FactsApp::advance_reader()
{
    if (page_index_ + 1U < page_count_) {
        ++page_index_;
        render_reader();
        return;
    }

    if (random_mode_) {
        open_random_fact();
        return;
    }

    current_fact_index_ = static_cast<std::uint8_t>(
        (current_fact_index_ + 1U)
        % data::kFactsPerCategory);
    title_selection_ = current_fact_index_;
    open_fact(category_, current_fact_index_, false);
}

std::uint8_t FactsApp::text_line_height() const
{
    return settings_.font_mode == font_core::Mode::Standard ? 24U :
        settings_.font_mode == font_core::Mode::PreRendered ? 24U : 19U;
}

std::uint16_t FactsApp::text_width(const char* text) const
{
    if (settings_.font_mode == font_core::Mode::Standard) {
        return static_cast<std::uint16_t>(
            2 * board_.polish_ui_text_width(text));
    }
    auto& renderer = const_cast<font_core::Renderer&>(renderer_);
    renderer.select(settings_.font_mode);
    renderer.set_size(font_core::Size::Readable);
    return renderer.measure(text, 30000, 30000).width_px;
}

bool FactsApp::wrap_pixels(const char* text, std::size_t start,
    std::size_t end, std::uint16_t max_width, std::uint8_t max_lines,
    char* dest, std::size_t dest_size) const
{
    if (!text || !dest || dest_size == 0 || start > end) return false;
    dest[0] = '\0';
    std::size_t out = 0;
    std::size_t cursor = start;
    std::size_t line_start = 0;
    std::uint8_t lines = 1;
    while (cursor < end) {
        while (cursor < end && text[cursor] == ' ') ++cursor;
        if (cursor >= end) break;
        std::size_t word_end = cursor;
        while (word_end < end && text[word_end] != ' ') {
            const std::size_t step = utf8_codepoint_bytes(
                static_cast<unsigned char>(text[word_end]));
            word_end += step <= end - word_end ? step : 1U;
        }
        const std::size_t bytes = word_end - cursor;
        const bool has_word = out > line_start;
        const std::size_t candidate = out + (has_word ? 1U : 0U) + bytes;
        if (candidate >= dest_size || candidate >= measure_scratch_.size())
            return false;
        const std::size_t existing = out - line_start;
        std::memcpy(measure_scratch_.data(), dest + line_start, existing);
        std::size_t n = existing;
        if (has_word) measure_scratch_[n++] = ' ';
        std::memcpy(measure_scratch_.data() + n, text + cursor, bytes);
        n += bytes;
        measure_scratch_[n] = '\0';
        if (text_width(measure_scratch_.data()) > max_width) {
            if (!has_word || lines >= max_lines) return false;
            if (out + 1U + bytes >= dest_size) return false;
            dest[out++] = '\n';
            line_start = out;
            ++lines;
        } else if (has_word) {
            dest[out++] = ' ';
        }
        if (out + bytes >= dest_size) return false;
        std::memcpy(dest + out, text + cursor, bytes);
        out += bytes;
        dest[out] = '\0';
        cursor = word_end;
    }
    return true;
}

void FactsApp::render_readable(const char* text, std::int16_t x,
    std::int16_t y, std::int16_t width, std::int16_t height,
    board::DisplayColor foreground, board::DisplayColor background)
{
    if (settings_.font_mode == font_core::Mode::Standard) {
        board_.draw_polish_ui_text_region(
            x, y, width, height, text, 2, foreground, background);
        return;
    }
    renderer_.select(settings_.font_mode);
    renderer_.set_size(font_core::Size::Readable);
    const auto result = renderer_.draw(
        text, {x, y, width, height}, foreground, background);
    if (!result.fits || !result.supported) {
        board_.draw_text_region(x, y, width, height,
            "TEXT FIT ERROR", 1, board::DisplayColor::Danger, background);
    }
}

std::uint8_t FactsApp::title_line_count() const
{
    if (!current_fact_) return 1;
    if (!wrap_pixels(current_fact_->title, 0, range_length(current_fact_->title),
            232, 3, wrapped_title_.data(), wrapped_title_.size())) return 3;
    std::uint8_t lines = 1;
    for (const char* p = wrapped_title_.data(); *p; ++p)
        if (*p == '\n') ++lines;
    return lines;
}

std::uint8_t FactsApp::body_line_capacity() const
{
    const std::int16_t divider = static_cast<std::int16_t>(
        17 + title_line_count() * text_line_height() + 2);
    const std::int16_t available = static_cast<std::int16_t>(119 - divider - 4);
    return static_cast<std::uint8_t>(std::max<std::int16_t>(
        1, available / text_line_height()));
}

void FactsApp::build_pages()
{
    page_count_ = 0;
    page_index_ = 0;
    page_overflow_ = false;

    if (current_fact_ == nullptr || current_fact_->text == nullptr) {
        return;
    }

    const char* text = current_fact_->text;
    const std::size_t end = range_length(text);

    // Greedy measured pagination: all text stays on one page when it fits.
    // Only break at whole-word UTF-8 byte boundaries when necessary.
    append_paginated_range(0U, end);

    if (page_count_ == 0U) {
        pages_[0] = PageRange{0U, static_cast<std::uint16_t>(end)};
        page_count_ = 1U;
    }
}

void FactsApp::append_paginated_range(std::size_t start, std::size_t end)
{
    if (!current_fact_ || start >= end) return;
    const char* text = current_fact_->text;
    std::size_t cursor = start;
    while (cursor < end) {
        while (cursor < end && text[cursor] == ' ') ++cursor;
        if (cursor == end) break;
        if (page_count_ >= kMaxPages) {
            page_overflow_ = true;
            return;
        }
        std::size_t candidate_end = cursor;
        std::size_t accepted_end = cursor;
        while (candidate_end < end) {
            while (candidate_end < end && text[candidate_end] == ' ')
                ++candidate_end;
            std::size_t word_end = candidate_end;
            while (word_end < end && text[word_end] != ' ') {
                const std::size_t step = utf8_codepoint_bytes(
                    static_cast<unsigned char>(text[word_end]));
                word_end += step <= end - word_end ? step : 1U;
            }
            if (word_end == candidate_end) break;
            if (!wrap_pixels(text, cursor, word_end, 232, body_line_capacity(),
                    wrapped_body_.data(), wrapped_body_.size())) break;
            accepted_end = word_end;
            candidate_end = word_end;
        }
        if (accepted_end == cursor) {
            page_overflow_ = true;
            return;
        }
        pages_[page_count_++] = PageRange{
            static_cast<std::uint16_t>(cursor),
            static_cast<std::uint16_t>(accepted_end)
        };
        cursor = accepted_end;
    }
}

void FactsApp::render_main_menu()
{
    board_.clear_screen();
    board_.draw_polish_ui_text_region(
        14, 4, 212, 19, "CIEKAWOSTKI", 1);
    board_.draw_line(14, 25, 225, 25, board::DisplayColor::SecondaryText);
    for (std::uint8_t slot = 0; slot < kMainVisibleRows; ++slot) {
        const std::uint8_t index =
            static_cast<std::uint8_t>(main_view_start_ + slot);
        if (index >= kMainItemCount) break;
        const bool selected = index == main_selection_;
        const std::int16_t y =
            static_cast<std::int16_t>(29 + slot * 29);
        const auto color = selected ? board::DisplayColor::PrimaryText
                                    : board::DisplayColor::SecondaryText;
        if (selected)
            board_.fill_rect(12, y, 224, 26, board::DisplayColor::Surface);
        if (wrap_pixels(kMainLabels[index], 0,
                range_length(kMainLabels[index]), 224, 1,
                wrapped_title_.data(), wrapped_title_.size())) {
            render_readable(wrapped_title_.data(), 12, y, 224, 26,
                color, selected ? board::DisplayColor::Surface
                                : board::DisplayColor::Background);
        } else {
            board_.draw_text_region(12, y, 224, 22,
                "TEXT FIT ERROR", 1, board::DisplayColor::Danger);
        }
        if (selected) draw_selection_marker(board_, 8, y, 24);
    }
    board_.draw_text_region(14, 121, 212, 12,
        "M5 WYBIERZ | BOCZNY DALEJ", 1);
}

void FactsApp::render_title_list()
{
    board_.clear_screen();
    board_.draw_polish_ui_text_region(12, 4, 168, 15,
        data::category_label(category_), 1);
    char position[16]{};
    std::snprintf(position, sizeof(position), "%03u/101",
        static_cast<unsigned>(title_selection_ + 1U));
    board_.draw_text_region(176, 4, 60, 14, position, 1,
        board::DisplayColor::Accent);
    board_.draw_line(10, 23, 230, 23, board::DisplayColor::SecondaryText);
    const data::Fact* facts = data::category_facts(category_);
    std::int16_t y = 28;
    for (std::uint8_t index = title_view_start_; index <= kTitleBackIndex; ++index) {
        const std::uint8_t height = title_item_height(index);
        if (y + height > 115) break;
        const char* label = index == kTitleBackIndex ? "POWRÓT" : facts[index].title;
        const bool selected = index == title_selection_;
        if (!wrap_pixels(label, 0, range_length(label), 224, 3,
                wrapped_title_.data(), wrapped_title_.size())) break;
        const auto bg = selected ? board::DisplayColor::Surface
                                 : board::DisplayColor::Background;
        render_readable(wrapped_title_.data(), 12, y, 224, height, 
            selected ? board::DisplayColor::PrimaryText
                     : board::DisplayColor::SecondaryText, bg);
        if (selected) draw_selection_marker(board_, 8, y, height - 2);
        y += height;
    }
    board_.draw_text_region(14, 121, 212, 12,
        "M5 OTWORZ | BOCZNY DALEJ", 1);
}

void FactsApp::render_reader()
{
    board_.clear_screen();

    if (current_fact_ == nullptr || page_count_ == 0U || page_overflow_) {
        board_.draw_text_region(
            50,
            55,
            160,
            20,
            "BRAK DANYCH",
            2,
            board::DisplayColor::Danger,
            board::DisplayColor::Background);
        return;
    }

    char fact_progress[12]{};
    if (random_mode_) {
        const std::uint16_t flat_index =
            static_cast<std::uint16_t>(
                category_number(category_) * data::kFactsPerCategory
                + current_fact_index_);
        std::snprintf(
            fact_progress,
            sizeof(fact_progress),
            "%03u/600",
            static_cast<unsigned>(flat_index + 1U));
    } else {
        std::snprintf(
            fact_progress,
            sizeof(fact_progress),
            "%03u/100",
            static_cast<unsigned>(current_fact_index_ + 1U));
    }

    board_.draw_polish_ui_text_region(
        4,
        2,
        146,
        12,
        random_mode_
            ? "LOSUJ"
            : data::category_label(category_),
        1,
        board::DisplayColor::SecondaryText,
        board::DisplayColor::Background);
    board_.draw_text_region(
        181,
        2,
        55,
        12,
        fact_progress,
        1,
        board::DisplayColor::SecondaryText,
        board::DisplayColor::Background);

    const std::size_t title_end = range_length(current_fact_->title);
    if (!wrap_pixels(current_fact_->title, 0U, title_end,
            232, 3, wrapped_title_.data(), wrapped_title_.size())) {
        board_.draw_text_region(4, 20, 232, 20,
            "TITLE FIT ERROR", 1, board::DisplayColor::Danger);
        return;
    }
    const std::int16_t title_height =
        static_cast<std::int16_t>(title_line_count() * text_line_height());
    render_readable(wrapped_title_.data(), 4, 16, 232, title_height);
    const std::int16_t divider_y = static_cast<std::int16_t>(18 + title_height);
    board_.draw_line(4, divider_y, 235, divider_y,
        board::DisplayColor::SecondaryText);
    const std::int16_t body_y = static_cast<std::int16_t>(divider_y + 3);
    const std::int16_t body_height = static_cast<std::int16_t>(119 - body_y);
    const PageRange& page = pages_[page_index_];
    if (!wrap_pixels(current_fact_->text, page.start, page.end,
            232, body_line_capacity(),
            wrapped_body_.data(), wrapped_body_.size())) {
        board_.draw_text_region(4, body_y, 232, 18,
            "BODY FIT ERROR", 1, board::DisplayColor::Danger);
        return;
    }
    render_readable(wrapped_body_.data(), 4, body_y, 232, body_height);

    char page_progress[12]{};
    std::snprintf(
        page_progress,
        sizeof(page_progress),
        "%u/%u",
        static_cast<unsigned>(page_index_ + 1U),
        static_cast<unsigned>(page_count_));
    board_.draw_text_region(
        5,
        123,
        40,
        11,
        page_progress,
        1,
        board::DisplayColor::Accent,
        board::DisplayColor::Background);
    board_.draw_text_region(
        133,
        123,
        103,
        11,
        page_index_ + 1U < page_count_
            ? "M5 DALEJ"
            : (random_mode_ ? "M5 LOSUJ" : "M5 NASTEPNY"),
        1,
        board::DisplayColor::SecondaryText,
        board::DisplayColor::Background);
}

}  // namespace nikos::facts
