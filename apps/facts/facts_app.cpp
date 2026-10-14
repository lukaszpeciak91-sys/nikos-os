#include "facts/facts_app.hpp"

#include <cstdio>
#include <cstring>

#include "esp_random.h"

namespace {

constexpr const char* kMainLabels[8] = {
    "LOSUJ",
    "KOSMOS",
    "ZWIERZĘTA",
    "TECHNOLOGIA",
    "CZŁOWIEK",
    "HISTORIA",
    "ŚWIAT",
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

FactsApp::FactsApp(board::Board& board)
    : board_(board)
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
        case 2:
            return data::Category::Animals;
        case 3:
            return data::Category::Technology;
        case 4:
            return data::Category::Human;
        case 5:
            return data::Category::History;
        case 6:
            return data::Category::World;
        case 1:
        default:
            return data::Category::Space;
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

void FactsApp::update_title_viewport()
{
    if (title_selection_ < title_view_start_) {
        title_view_start_ = title_selection_;
    } else if (
        title_selection_
        >= static_cast<std::uint8_t>(
            title_view_start_ + kTitleVisibleRows)) {
        title_view_start_ = static_cast<std::uint8_t>(
            title_selection_ - kTitleVisibleRows + 1U);
    }

    constexpr std::uint8_t kItemCount =
        kTitleBackIndex + 1U;
    const std::uint8_t max_start =
        kItemCount - kTitleVisibleRows;
    if (title_view_start_ > max_start) {
        title_view_start_ = max_start;
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

void FactsApp::build_pages()
{
    page_count_ = 0;
    page_index_ = 0;

    if (current_fact_ == nullptr || current_fact_->text == nullptr) {
        return;
    }

    const char* text = current_fact_->text;
    const std::size_t end = range_length(text);

    std::size_t preferred_boundary = 0;
    for (std::size_t i = 0; i + 1U < end; ++i) {
        const char current = text[i];
        if ((current == '.' || current == '!' || current == '?')
            && text[i + 1U] == ' ') {
            preferred_boundary = i + 1U;
        }
    }

    if (preferred_boundary > 0U
        && preferred_boundary < end) {
        append_paginated_range(0U, preferred_boundary);

        std::size_t second_start = preferred_boundary;
        while (second_start < end && text[second_start] == ' ') {
            ++second_start;
        }
        append_paginated_range(second_start, end);
    } else {
        append_paginated_range(0U, end);
    }

    if (page_count_ == 0U) {
        pages_[0] = PageRange{0U, static_cast<std::uint16_t>(end)};
        page_count_ = 1U;
    }
}

void FactsApp::append_paginated_range(
    std::size_t start,
    std::size_t end)
{
    if (current_fact_ == nullptr || start >= end) {
        return;
    }

    const char* text = current_fact_->text;
    std::size_t cursor = start;

    while (cursor < end && page_count_ < kMaxPages) {
        while (cursor < end && text[cursor] == ' ') {
            ++cursor;
        }
        if (cursor >= end) {
            break;
        }

        std::size_t page_end = end;
        if (count_utf8_codepoints(text, cursor, end)
            > kPageMaxCodepoints) {
            page_end = bounded_word_split(
                text,
                cursor,
                end,
                kPageMaxCodepoints);
        }

        while (page_end > cursor && text[page_end - 1U] == ' ') {
            --page_end;
        }

        pages_[page_count_++] = PageRange{
            static_cast<std::uint16_t>(cursor),
            static_cast<std::uint16_t>(page_end),
        };

        cursor = page_end;
        while (cursor < end && text[cursor] == ' ') {
            ++cursor;
        }
    }
}

void FactsApp::render_main_menu()
{
    board_.clear_screen();

    board_.draw_polish_ui_text_region(
        14,
        6,
        212,
        16,
        "CIEKAWOSTKI",
        1,
        board::DisplayColor::PrimaryText,
        board::DisplayColor::Background);
    board_.draw_line(
        14,
        23,
        225,
        23,
        board::DisplayColor::SecondaryText);

    for (std::uint8_t slot = 0; slot < kMainVisibleRows; ++slot) {
        const std::uint8_t index =
            static_cast<std::uint8_t>(main_view_start_ + slot);
        if (index >= kMainItemCount) {
            break;
        }

        const bool selected = index == main_selection_;
        const std::int16_t y =
            static_cast<std::int16_t>(29 + slot * 18);

        board_.draw_polish_ui_text_region(
            26,
            y,
            194,
            16,
            kMainLabels[index],
            1,
            selected
                ? board::DisplayColor::PrimaryText
                : board::DisplayColor::SecondaryText,
            selected
                ? board::DisplayColor::Surface
                : board::DisplayColor::Background);

        if (selected) {
            draw_selection_marker(board_, 16, y, 14);
        }
    }

    board_.draw_text_region(
        14,
        121,
        212,
        12,
        "M5 WYBIERZ | BOCZNY DALEJ",
        1,
        board::DisplayColor::SecondaryText,
        board::DisplayColor::Background);
}

void FactsApp::render_title_list()
{
    board_.clear_screen();

    board_.draw_polish_ui_text_region(
        12,
        4,
        180,
        15,
        data::category_label(category_),
        1,
        board::DisplayColor::PrimaryText,
        board::DisplayColor::Background);

    char position[12]{};
    if (title_selection_ < data::kFactsPerCategory) {
        std::snprintf(
            position,
            sizeof(position),
            "%03u/100",
            static_cast<unsigned>(title_selection_ + 1U));
    }
    board_.draw_text_region(
        186,
        4,
        50,
        12,
        position,
        1,
        board::DisplayColor::SecondaryText,
        board::DisplayColor::Background);

    const data::Fact* facts =
        data::category_facts(category_);

    for (std::uint8_t slot = 0; slot < kTitleVisibleRows; ++slot) {
        const std::uint8_t index =
            static_cast<std::uint8_t>(title_view_start_ + slot);
        if (index > kTitleBackIndex) {
            break;
        }

        const bool selected = index == title_selection_;
        const std::int16_t y =
            static_cast<std::int16_t>(23 + slot * 19);
        const char* label =
            index == kTitleBackIndex
                ? "POWRÓT"
                : facts[index].title;

        board_.draw_polish_ui_text_region(
            18,
            y,
            214,
            17,
            label,
            1,
            selected
                ? board::DisplayColor::PrimaryText
                : board::DisplayColor::SecondaryText,
            selected
                ? board::DisplayColor::Surface
                : board::DisplayColor::Background);

        if (selected) {
            draw_selection_marker(board_, 10, y, 15);
        }
    }

    board_.draw_text_region(
        14,
        121,
        212,
        12,
        "M5 OTWORZ | BOCZNY DALEJ",
        1,
        board::DisplayColor::SecondaryText,
        board::DisplayColor::Background);
}

void FactsApp::render_reader()
{
    board_.clear_screen();

    if (current_fact_ == nullptr || page_count_ == 0U) {
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
    std::snprintf(
        fact_progress,
        sizeof(fact_progress),
        "%03u/100",
        static_cast<unsigned>(current_fact_index_ + 1U));

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

    const std::size_t title_end =
        range_length(current_fact_->title);
    if (!wrap_utf8(
            current_fact_->title,
            0U,
            title_end,
            kTitleCodepointsPerLine,
            kTitleMaxLines,
            wrapped_title_.data(),
            wrapped_title_.size())) {
        std::snprintf(
            wrapped_title_.data(),
            wrapped_title_.size(),
            "%s",
            current_fact_->title);
    }

    board_.draw_polish_ui_text_region(
        4,
        15,
        232,
        27,
        wrapped_title_.data(),
        1,
        board::DisplayColor::PrimaryText,
        board::DisplayColor::Background);

    board_.draw_line(
        4,
        43,
        235,
        43,
        board::DisplayColor::SecondaryText);

    const PageRange& page = pages_[page_index_];
    if (!wrap_utf8(
            current_fact_->text,
            page.start,
            page.end,
            kBodyCodepointsPerLine,
            kBodyMaxLines,
            wrapped_body_.data(),
            wrapped_body_.size())) {
        wrapped_body_[0] = '\0';
    }

    board_.draw_polish_ui_text_region(
        4,
        47,
        232,
        73,
        wrapped_body_.data(),
        1,
        board::DisplayColor::PrimaryText,
        board::DisplayColor::Background);

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
