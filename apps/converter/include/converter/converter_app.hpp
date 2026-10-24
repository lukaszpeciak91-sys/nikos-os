#pragma once
#include "board/board.hpp"
#include "font_core/font_core.hpp"
#include "converter/conversion.hpp"

namespace nikos::converter {
class ConverterApp final {
public:
    enum class Result : std::uint8_t { Running, ExitRequested };
    explicit ConverterApp(board::Board& board):board_(board),font_(board){}
    void begin();
    void redraw();
    void suspend(){suspended_=true;}
    void resume(){suspended_=false;redraw();}
    void end(){active_=false;}
    Result update(const board::InputState& input);
private:
    enum class Screen : std::uint8_t { Categories, Conversion, Editing, Units };
    enum class Focus : std::uint8_t { Value, From, To };
    void render();
    void render_conversion();
    void render_editor();
    void render_units();
    void draw(const char* text,std::int16_t x,std::int16_t y,std::int16_t w,
        board::DisplayColor color=board::DisplayColor::PrimaryText);
    void draw_fitted(const char* text,std::int16_t x,std::int16_t y,
        std::int16_t w,board::DisplayColor color,
        board::DisplayColor background=board::DisplayColor::Background);
    void focus_box(std::int16_t x,std::int16_t y,
        std::int16_t w,std::int16_t h,bool selected);
    void choose_category();
    board::Board& board_;
    font_core::Renderer font_;
    Screen screen_=Screen::Categories;
    std::uint8_t selection_=0;
    Category category_=Category::Temperature;
    Focus focus_=Focus::Value;
    Focus unit_target_=Focus::From;
    std::uint8_t from_unit_=0, to_unit_=1, candidate_unit_=0;
    // Keep the entered physical value independent from the source unit.
    double input_value_=20.0;
    double base_=293.15;
    Editor editor_{};
    bool save_focus_=false;
    bool active_=false,suspended_=false;
    bool invalid_temperature_=false;
    bool invalid_unit_=false;
};
}
