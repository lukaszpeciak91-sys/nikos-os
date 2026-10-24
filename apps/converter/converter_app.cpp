#include "converter/converter_app.hpp"

#include <cstdio>

namespace nikos::converter {
namespace {
constexpr const char* kTitles[] = {"TEMPERATURA", "DŁUGOŚĆ", "MASA", "OBJĘTOŚĆ"};
// Start in the familiar physical quantities, with easy-to-read base units.
constexpr std::uint8_t kDefaultFrom[] = {0, 2, 1, 1}; // °C, m, kg, l
constexpr std::uint8_t kDefaultTo[] = {1, 1, 0, 0};   // °F, cm, g, ml
constexpr double kDefaultValues[] = {20.0, 1.0, 1.0, 1.0};
}

void ConverterApp::begin()
{
    active_ = true;
    suspended_ = false;
    screen_ = Screen::Categories;
    selection_ = 0;
    focus_ = Focus::Value;
    invalid_temperature_ = false;
    invalid_unit_ = false;
    save_focus_ = false;
    render();
}

void ConverterApp::redraw()
{
    if (active_ && !suspended_) render();
}

void ConverterApp::draw(const char* text, std::int16_t x, std::int16_t y,
                        std::int16_t w, board::DisplayColor color,
                        board::DisplayColor background)
{
    font_.select(font_core::Mode::PreRendered);
    font_.set_size(font_core::Size::Compact);
    font_.draw(text, {x, y, w, 19}, color, background);
}

void ConverterApp::draw_fitted(const char* text, std::int16_t x,
    std::int16_t y, std::int16_t w, board::DisplayColor color,
    board::DisplayColor background)
{
    font_.select(font_core::Mode::PreRendered);
    font_.set_size(font_core::Size::Readable);
    const auto readable = font_.measure(text, w, 24);
    if (readable.supported && readable.fits && readable.lines == 1) {
        font_.draw(text, {x, y, w, 24}, color, background);
        return;
    }
    font_.set_size(font_core::Size::Compact);
    const auto compact = font_.measure(text, w, 19);
    const char* shown = compact.supported && compact.fits
        && compact.lines == 1 ? text : "POZA ZAKR.";
    font_.draw(shown, {x, y, w, 19}, color, background);
}

void ConverterApp::focus_box(std::int16_t x, std::int16_t y,
    std::int16_t w, std::int16_t h, bool selected)
{
    board_.fill_rect(x, y, w, h, selected
        ? board::DisplayColor::Surface : board::DisplayColor::Background);
    if (selected) board_.fill_rect(x, y, 3, h, board::DisplayColor::Accent);
}

void ConverterApp::choose_category()
{
    category_ = static_cast<Category>(selection_);
    from_unit_ = kDefaultFrom[selection_];
    to_unit_ = kDefaultTo[selection_];
    input_value_ = kDefaultValues[selection_];
    (void)to_base(category_, from_unit_, input_value_, base_);
    focus_ = Focus::Value;
    invalid_temperature_ = false;
    invalid_unit_ = false;
    screen_ = Screen::Conversion;
}

ConverterApp::Result ConverterApp::update(const board::InputState& input)
{
    if (!active_) return Result::ExitRequested;
    if (suspended_) return Result::Running;

    if (input.secondary_long) {
        if (screen_ == Screen::Categories) return Result::ExitRequested;
        if (screen_ == Screen::Conversion) {
            screen_ = Screen::Categories;
        } else {
            // Editing and unit picking are transactional: discard the draft.
            screen_ = Screen::Conversion;
            editor_ = Editor{};
            save_focus_ = false;
        }
        invalid_temperature_ = false;
        invalid_unit_ = false;
        render();
        return Result::Running;
    }

    if (screen_ == Screen::Categories) {
        if (input.secondary_short) {
            selection_ = static_cast<std::uint8_t>((selection_ + 1U) % 5U);
            render();
        } else if (input.primary_short) {
            if (selection_ == 4U) return Result::ExitRequested;
            choose_category();
            render();
        }
    } else if (screen_ == Screen::Conversion) {
        if (input.secondary_short) {
            focus_ = static_cast<Focus>(
                (static_cast<std::uint8_t>(focus_) + 1U) % 3U);
            render();
        } else if (input.primary_short) {
            if (focus_ == Focus::Value) {
                if (!editable_value(category_, input_value_))
                    return Result::Running;
                editor_.begin(input_value_, category_ == Category::Temperature);
                save_focus_ = false;
                invalid_temperature_ = false;
                screen_ = Screen::Editing;
            } else {
                unit_target_ = focus_;
                candidate_unit_ = unit_target_ == Focus::From
                    ? from_unit_ : to_unit_;
                invalid_unit_ = false;
                screen_ = Screen::Units;
            }
            render();
        }
    } else if (screen_ == Screen::Units) {
        if (input.secondary_short) {
            candidate_unit_ = static_cast<std::uint8_t>(
                (candidate_unit_ + 1U) % count(category_));
            invalid_unit_ = false;
            render();
        } else if (input.primary_short) {
            if (unit_target_ == Focus::From) {
                // A source-unit change reinterprets the same entered number.
                // Reject a physically impossible temperature without mutation.
                double next_base = 0;
                if (!to_base(category_, candidate_unit_, input_value_, next_base)) {
                    invalid_unit_ = true;
                    render();
                    return Result::Running;
                }
                from_unit_ = candidate_unit_;
                base_ = next_base;
            } else {
                to_unit_ = candidate_unit_;
            }
            invalid_unit_ = false;
            screen_ = Screen::Conversion;
            render();
        }
    } else if (screen_ == Screen::Editing) {
        if (input.secondary_short) {
            if (save_focus_) {
                save_focus_ = false;
                editor_.cursor = 0;
            } else {
                editor_.increment();
            }
            invalid_temperature_ = false;
            render();
        } else if (input.primary_short) {
            if (!save_focus_) {
                if (editor_.next()) {
                    editor_.cursor = 0;
                    save_focus_ = true;
                }
            } else {
                double next_base = 0;
                const double entered = editor_.value();
                if (to_base(category_, from_unit_, entered, next_base)) {
                    input_value_ = entered;
                    base_ = next_base;
                    screen_ = Screen::Conversion;
                    invalid_temperature_ = false;
                    save_focus_ = false;
                } else {
                    // Keep the entire invalid draft available for correction.
                    invalid_temperature_ = true;
                    save_focus_ = false;
                    editor_.cursor = 0;
                }
            }
            render();
        }
    }
    return Result::Running;
}

void ConverterApp::render()
{
    board_.clear_screen();
    if (screen_ == Screen::Categories) {
        draw("KONWERTER", 14, 5, 210);
        const std::uint8_t first = selection_ >= 3U
            ? static_cast<std::uint8_t>(selection_ - 2U) : 0U;
        for (std::uint8_t slot = 0; slot < 3U; ++slot) {
            const auto index = static_cast<std::uint8_t>(first + slot);
            const auto y = static_cast<std::int16_t>(31 + slot * 27);
            if (index == selection_) {
                board_.fill_rect(16, y, 208, 23, board::DisplayColor::Surface);
                board_.fill_rect(16, y, 3, 23, board::DisplayColor::Accent);
            }
            font_.select(font_core::Mode::PreRendered);
            font_.set_size(font_core::Size::Readable);
            font_.draw(index == 4U ? "POWRÓT" : kTitles[index],
                {22, y, 195, 24},
                index == selection_ ? board::DisplayColor::PrimaryText
                                    : board::DisplayColor::SecondaryText,
                index == selection_ ? board::DisplayColor::Surface
                                    : board::DisplayColor::Background);
        }
        board_.draw_text_region(12, 119, 218, 16,
            "M5 WYBIERZ  SIDE DALEJ", 1);
        return;
    }

    if (screen_ == Screen::Conversion) {
        render_conversion();
    } else if (screen_ == Screen::Editing) {
        render_editor();
    } else {
        render_units();
    }
}

void ConverterApp::render_conversion()
{
    draw(kTitles[static_cast<std::uint8_t>(category_)], 12, 1, 216);

    focus_box(12, 22, 216, 26, focus_ == Focus::Value);
    const auto input_background = focus_ == Focus::Value
        ? board::DisplayColor::Surface : board::DisplayColor::Background;
    draw("WARTOŚĆ", 20, 26, 100, board::DisplayColor::SecondaryText,
        input_background);
    char number[24]{};
    format(input_value_, number);
    draw_fitted(number, 123, 23, 101,
        board::DisplayColor::PrimaryText, input_background);

    focus_box(12, 49, 103, 24, focus_ == Focus::From);
    focus_box(120, 49, 108, 24, focus_ == Focus::To);
    const auto from_background = focus_ == Focus::From
        ? board::DisplayColor::Surface : board::DisplayColor::Background;
    const auto to_background = focus_ == Focus::To
        ? board::DisplayColor::Surface : board::DisplayColor::Background;
    draw("Z", 20, 52, 21, board::DisplayColor::SecondaryText, from_background);
    draw(units(category_)[from_unit_].label, 45, 52, 68,
        board::DisplayColor::PrimaryText, from_background);
    draw("NA", 128, 52, 35, board::DisplayColor::SecondaryText, to_background);
    draw(units(category_)[to_unit_].label, 164, 52, 61,
        board::DisplayColor::PrimaryText, to_background);

    draw("WYNIK", 12, 74, 212, board::DisplayColor::SecondaryText);
    double result = 0;
    char shown[24]{};
    if (from_base(category_, to_unit_, base_, result)) {
        format(result, shown);
    } else {
        std::snprintf(shown, sizeof(shown), "POZA ZAKR.");
    }
    char result_text[48]{};
    if (shown[0] == 'P') {
        std::snprintf(result_text, sizeof(result_text), "%s", shown);
    } else {
        std::snprintf(result_text, sizeof(result_text),
            "%s %s", shown, units(category_)[to_unit_].label);
    }
    draw_fitted(result_text, 17, 94, 210, board::DisplayColor::PrimaryText);

    board_.draw_text_region(12, 119, 218, 16,
        "M5 WYBIERZ  SIDE DALEJ", 1);
}

void ConverterApp::render_units()
{
    draw(unit_target_ == Focus::From ? "WYBIERZ Z" : "WYBIERZ NA",
        12, 3, 216);
    const std::uint8_t first = candidate_unit_ >= 3U
        ? static_cast<std::uint8_t>(candidate_unit_ - 2U) : 0U;
    for (std::uint8_t slot = 0; slot < 3U; ++slot) {
        const auto index = static_cast<std::uint8_t>(first + slot);
        if (index >= count(category_)) break;
        const auto y = static_cast<std::int16_t>(29 + slot * 27);
        const bool selected = index == candidate_unit_;
        focus_box(16, y, 208, 24, selected);
        font_.select(font_core::Mode::PreRendered);
        font_.set_size(font_core::Size::Readable);
        font_.draw(units(category_)[index].label, {25, y, 192, 24},
            selected ? board::DisplayColor::PrimaryText
                     : board::DisplayColor::SecondaryText,
            selected ? board::DisplayColor::Surface
                     : board::DisplayColor::Background);
    }
    if (invalid_unit_) {
        draw("PONIŻEJ ZERA ABS.", 12, 115, 216,
            board::DisplayColor::Attention);
    } else {
        board_.draw_text_region(12, 119, 218, 16,
            "M5 OK  SIDE JEDN.  DL ANULUJ", 1);
    }
}

void ConverterApp::render_editor()
{
    draw("WARTOŚĆ", 12, 3, 216);
    char edit[9]{};
    editor_.text(edit);
    font_.select(font_core::Mode::PreRendered);
    font_.set_size(font_core::Size::Readable);

    std::int16_t total_width = 0;
    for (const char* p = edit; *p; ++p)
        total_width = static_cast<std::int16_t>(
            total_width + font_.advance(static_cast<std::uint8_t>(*p)));
    std::int16_t x = static_cast<std::int16_t>((240 - total_width) / 2);
    const std::uint8_t selected = editor_.character_index();
    // Preserve the fixed-digit editor, but visually quiet unused leading zeros.
    const std::uint8_t first_digit = editor_.temperature ? 1U : 0U;
    std::uint8_t significant = first_digit + 3U;
    for (std::uint8_t i = first_digit; i < first_digit + 4U; ++i) {
        if (edit[i] != '0') { significant = i; break; }
    }
    for (std::uint8_t i = 0; edit[i] != '\0'; ++i) {
        const auto width = static_cast<std::int16_t>(
            font_.advance(static_cast<std::uint8_t>(edit[i])));
        const bool focused = !save_focus_ && i == selected;
        const char character[] = {edit[i], '\0'};
        const auto color = focused ? board::DisplayColor::PrimaryText
            : i >= first_digit && i < significant
                ? board::DisplayColor::SecondaryText
                : board::DisplayColor::Accent;
        font_.draw(character, {x, 34, width, 24}, color,
            focused ? board::DisplayColor::Surface
                    : board::DisplayColor::Background);
        if (focused)
            board_.fill_rect(x, 60, width, 3, board::DisplayColor::Accent);
        x = static_cast<std::int16_t>(x + width);
    }

    draw(units(category_)[from_unit_].label, 32, 67, 176,
        board::DisplayColor::SecondaryText);
    focus_box(71, 87, 98, 25, save_focus_);
    draw("ZAPISZ", 86, 90, 80,
        save_focus_ ? board::DisplayColor::PrimaryText
                    : board::DisplayColor::SecondaryText,
        save_focus_ ? board::DisplayColor::Surface
                    : board::DisplayColor::Background);

    if (invalid_temperature_) {
        draw("PONIŻEJ ZERA ABS.", 12, 115, 216,
            board::DisplayColor::Attention);
    } else {
        board_.draw_text_region(12, 119, 218, 16,
            save_focus_ ? "M5 ZAPISZ  SIDE POPRAW  DL ANULUJ"
                        : "SIDE +1  M5 DALEJ  DL ANULUJ", 1);
    }
}
}
