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
    enum class Screen : std::uint8_t { Categories, Conversion, Editing };
    void render();
    void draw(const char* text,std::int16_t x,std::int16_t y,std::int16_t w,
        board::DisplayColor color=board::DisplayColor::PrimaryText);
    void choose_category();
    board::Board& board_;
    font_core::Renderer font_;
    Screen screen_=Screen::Categories;
    std::uint8_t selection_=0;
    Category category_=Category::Temperature;
    std::uint8_t unit_=0;
    double base_=293.15;
    Editor editor_{};
    bool active_=false,suspended_=false;
    bool invalid_temperature_=false;
};
}
