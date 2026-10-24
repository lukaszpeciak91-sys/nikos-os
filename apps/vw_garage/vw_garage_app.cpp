#include "vw_garage/vw_garage_app.hpp"

#include <cstdio>
#include "vw_garage/catalogue.hpp"

namespace nikos::vw_garage {
VwGarageApp::VwGarageApp(board::Board& board)
    : board_(board), renderer_(board) {
    renderer_.select(font_core::Mode::PreRendered);
    renderer_.set_size(font_core::Size::Compact);
}
void VwGarageApp::begin() {
    selected_index_ = 0;
    viewport_start_ = 0;
    view_ = View::List;
    active_ = true;
    redraw();
}
void VwGarageApp::end() { active_ = false; }
void VwGarageApp::ensure_selection_visible() {
    if (selected_index_ < viewport_start_) {
        viewport_start_ = selected_index_;
    } else if (selected_index_ >= viewport_start_ + kVisibleRows) {
        viewport_start_ = static_cast<std::uint8_t>(
            selected_index_ - kVisibleRows + 1U);
    }
}
void VwGarageApp::return_to_list() {
    view_ = View::List;
    ensure_selection_visible();
    render_list();
}
VwGarageApp::UpdateResult VwGarageApp::update(const board::InputState& input) {
    if (!active_) return UpdateResult::ExitRequested;
    if (input.secondary_long) {
        if (view_ == View::List) return UpdateResult::ExitRequested;
        return_to_list();
    } else if (input.secondary_short) {
        if (view_ == View::List) {
            selected_index_ = static_cast<std::uint8_t>(
                (selected_index_ + 1U) % kModelCount);
            ensure_selection_visible();
            render_list();
        } else {
            return_to_list();
        }
    } else if (input.primary_short) {
        if (view_ == View::List) {
            view_ = View::Card;
        } else {
            selected_index_ = static_cast<std::uint8_t>(
                (selected_index_ + 1U) % kModelCount);
        }
        render_card();
    }
    return UpdateResult::Running;
}
void VwGarageApp::redraw() {
    if (!active_) return;
    if (view_ == View::Card) render_card();
    else render_list();
}
void VwGarageApp::draw_header() {
    char counter[7];
    std::snprintf(counter, sizeof(counter), "%02u/25",
        static_cast<unsigned>(selected_index_ + 1U));
    renderer_.draw("GARAŻ VW", {12, 3, 156, 19},
        board::DisplayColor::PrimaryText);
    renderer_.draw(counter, {179, 3, 52, 19},
        board::DisplayColor::Accent);
    board_.draw_line(10, 24, 229, 24,
        board::DisplayColor::SecondaryText);
}
void VwGarageApp::render_list() {
    board_.clear_screen();
    draw_header();
    for (std::uint8_t slot = 0; slot < kVisibleRows; ++slot) {
        const std::uint8_t index = static_cast<std::uint8_t>(viewport_start_ + slot);
        if (index >= kModelCount) break;
        const auto& model = kModels[index];
        const bool selected = index == selected_index_;
        const std::int16_t y = static_cast<std::int16_t>(28 + 22 * slot);
        const auto bg = selected ? board::DisplayColor::Surface
                                 : board::DisplayColor::Background;
        const auto fg = selected ? board::DisplayColor::PrimaryText
                                 : board::DisplayColor::SecondaryText;
        board_.fill_rect(8, y, 224, 22, bg);
        if (selected) {
            board_.fill_rect(8, static_cast<std::int16_t>(y + 1),
                2, 20, board::DisplayColor::Accent);
        }
        renderer_.draw(model.name,
            {16, static_cast<std::int16_t>(y + 1), 164, 19}, fg, bg);
        char year[6];
        std::snprintf(year, sizeof(year), "%u",
            static_cast<unsigned>(model.debut_year));
        const auto width = renderer_.measure(year, 45, 19).width_px;
        renderer_.draw(year, {
            static_cast<std::int16_t>(231 - width),
            static_cast<std::int16_t>(y + 1),
            static_cast<std::int16_t>(width), 19
        }, board::DisplayColor::Accent, bg);
    }
    board_.draw_polish_ui_text_region(12, 121, 218, 12,
        "M5 OTWÓRZ | SIDE DALEJ", 1,
        board::DisplayColor::SecondaryText, board::DisplayColor::Background);
}
void VwGarageApp::render_card() {
    board_.clear_screen();
    draw_header();
    const auto& model = kModels[selected_index_];
    renderer_.draw(model.name, {12, 28, 216, 19}, board::DisplayColor::Accent);
    board_.draw_text_region(12, 57, 42, 12, "DEBIUT", 1,
        board::DisplayColor::SecondaryText, board::DisplayColor::Background);
    char year[6];
    std::snprintf(year, sizeof(year), "%u",
        static_cast<unsigned>(model.debut_year));
    renderer_.draw(year, {56, 53, 55, 19});
    board_.draw_text_region(113, 57, 24, 12, "TYP", 1,
        board::DisplayColor::SecondaryText, board::DisplayColor::Background);
    renderer_.draw(model.vehicle_type, {140, 53, 94, 19});
    renderer_.draw(model.description_pl, {12, 77, 216, 38});
    board_.draw_polish_ui_text_region(12, 122, 218, 12,
        "M5 NASTĘPNY | SIDE WRÓĆ", 1,
        board::DisplayColor::SecondaryText, board::DisplayColor::Background);
}
}  // namespace nikos::vw_garage
