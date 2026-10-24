#include <cassert>
#include <cmath>
#include <cstdint>
#include <cstring>

#include "board/board.hpp"
#include "font_core/font_core.hpp"
#include "converter/conversion.hpp"
#define private public
#include "converter/converter_app.hpp"
#undef private

using nikos::board::InputState;
using nikos::converter::Category;
using nikos::converter::ConverterApp;
using nikos::converter::to_base;
using nikos::converter::from_base;
using nikos::converter::count;

InputState m5() { InputState input{}; input.primary_short = true; return input; }
InputState side() { InputState input{}; input.secondary_short = true; return input; }
InputState back() { InputState input{}; input.secondary_long = true; return input; }
bool close(double a, double b, double e=1e-7) { return std::fabs(a-b)<e; }
void go_to(ConverterApp& app, std::uint8_t target) {
    // Exactly three focus positions: WARTOŚĆ, Z, NA.
    for (unsigned tries=0; app.focus_ != static_cast<ConverterApp::Focus>(target)
        && tries<3; ++tries) app.update(side());
    assert(app.focus_==static_cast<ConverterApp::Focus>(target));
}
void choose_unit(ConverterApp& app, unsigned focus, unsigned target) {
    using Screen=ConverterApp::Screen;
    go_to(app,static_cast<std::uint8_t>(focus));
    app.update(m5());
    assert(app.screen_==Screen::Units);
    const unsigned n=count(app.category_);
    for(unsigned i=0;i<n && app.candidate_unit_!=target;++i) app.update(side());
    assert(app.candidate_unit_==target);
    app.update(m5());
    assert(app.screen_==Screen::Conversion);
}
void open_editor(ConverterApp& app) {
    go_to(app,0);
    app.update(m5());
    assert(app.screen_==ConverterApp::Screen::Editing);
    assert(!app.save_focus_);
}
void focus_save(ConverterApp& app) {
    const auto count_positions=app.editor_.positions();
    for(unsigned i=0;i<count_positions;++i) {
        assert(!app.save_focus_);
        app.update(m5());
    }
    assert(app.save_focus_ && app.screen_==ConverterApp::Screen::Editing);
}
void select_category(ConverterApp& app,std::uint8_t category) {
    app.begin();
    for(std::uint8_t i=0;i<category;++i)app.update(side());
    assert(app.selection_==category);
    app.update(m5());
    assert(app.screen_==ConverterApp::Screen::Conversion);
    assert(static_cast<unsigned>(app.category_)==category);
    assert(app.focus_==ConverterApp::Focus::Value);
}

int main() {
    using Screen=ConverterApp::Screen;
    using Focus=ConverterApp::Focus;
    using Result=ConverterApp::Result;
    nikos::board::Board board;
    ConverterApp app(board);
    InputState power{}; power.power_short=true;

    app.begin();
    assert(app.screen_==Screen::Categories && app.selection_==0);
    const int before_power=board.clear_calls;
    assert(app.update(power)==Result::Running);
    assert(board.clear_calls==before_power);
    assert(app.update(back())==Result::ExitRequested);

    select_category(app,1); // Length, default 1 m -> 100 cm.
    assert(app.from_unit_==2 && app.to_unit_==1);
    assert(close(app.input_value_,1.) && close(app.base_,1.));
    assert(app.focus_==Focus::Value);
    app.update(side());
    assert(app.focus_==Focus::From);
    app.update(side());
    assert(app.focus_==Focus::To);
    app.update(side());
    assert(app.focus_==Focus::Value);

    // Cancel source picker after changing draft. Neither units nor value change.
    go_to(app,1);
    app.update(m5());
    assert(app.screen_==Screen::Units && app.candidate_unit_==2);
    app.update(side());
    app.update(back());
    assert(app.screen_==Screen::Conversion);
    assert(app.focus_==Focus::From && app.from_unit_==2);
    assert(close(app.input_value_,1.) && close(app.base_,1.));
    // Destination picker cancel equally transactional.
    go_to(app,2);
    app.update(m5());
    app.update(side());
    app.update(back());
    assert(app.screen_==Screen::Conversion && app.to_unit_==1);

    // Source switching: exactly the same confirmed numeric value is reinterpreted.
    choose_unit(app,1,1); // 1 cm, not 100 cm
    assert(app.from_unit_==1 && close(app.input_value_,1.));
    assert(close(app.base_,.01));
    choose_unit(app,2,1); // Identical source and destination is allowed.
    double result=0;
    assert(from_base(app.category_,app.to_unit_,app.base_,result) && close(result,1.));
    choose_unit(app,2,2); // 1 cm -> 0.01 m
    assert(close(app.base_,.01) && close(app.input_value_,1.));
    assert(from_base(app.category_,app.to_unit_,app.base_,result)&&close(result,.01));

    // A five-step digit traversal reaches ZAPISZ, but does not auto-commit.
    open_editor(app);
    assert(close(app.editor_.value(),1.));
    app.update(side()); // Thousands +1 = 1001 cm
    assert(close(app.editor_.value(),1001.));
    const int before_pause=board.clear_calls;
    app.suspend();
    assert(app.update(m5())==Result::Running);
    app.redraw();
    assert(board.clear_calls==before_pause);
    app.resume();
    assert(board.clear_calls==before_pause+1);
    assert(app.screen_==Screen::Editing && close(app.editor_.value(),1001.));
    focus_save(app);
    assert(close(app.input_value_,1.) && close(app.base_,.01));
    app.redraw();
    assert(app.save_focus_);
    // SIDE on ZAPISZ returns to the first digit for correction.
    app.update(side());
    assert(!app.save_focus_ && app.editor_.cursor==0);
    app.update(back());
    assert(app.screen_==Screen::Conversion && close(app.base_,.01));
    assert(close(app.input_value_,1.));
    open_editor(app);
    app.update(side()); // 1001 cm
    focus_save(app);
    app.update(m5());   // Explicit save only.
    assert(app.screen_==Screen::Conversion);
    assert(close(app.input_value_,1001.) && close(app.base_,10.01));
    assert(app.from_unit_==1 && app.to_unit_==2);

    // Pause/resume and redraw while selecting a unit.
    go_to(app,2);
    app.update(m5());
    const auto candidate=app.candidate_unit_;
    const int clears=board.clear_calls;
    app.suspend(); app.redraw(); app.update(side());
    assert(app.candidate_unit_==candidate && board.clear_calls==clears);
    app.resume(); assert(board.clear_calls==clears+1);
    app.update(back()); assert(app.screen_==Screen::Conversion);
    app.update(back()); assert(app.screen_==Screen::Categories);
    assert(app.selection_==1 && app.update(back())==Result::ExitRequested);

    // Every category and every available unit: source and destination are independent
    // and conversions use unchanged production numerical factors.
    for (std::uint8_t c=0;c<4;++c) {
        select_category(app,c);
        const double initial=app.input_value_;
        for (std::uint8_t src=0;src<count(app.category_);++src) {
            choose_unit(app,1,src);
            assert(app.from_unit_==src && close(app.input_value_,initial));
            double expected_base=0;
            assert(to_base(app.category_,src,initial,expected_base));
            assert(close(app.base_,expected_base));
            for (std::uint8_t dst=0;dst<count(app.category_);++dst) {
                choose_unit(app,2,dst);
                assert(app.to_unit_==dst);
                assert(close(app.input_value_,initial));
                assert(close(app.base_,expected_base));
                double actual=0;
                assert(from_base(app.category_,dst,app.base_,actual));
                if(src==dst)assert(close(actual,initial));
            }
        }
    }

    // Temperature negative input and absolute-zero protection.
    select_category(app,0);
    assert(close(app.input_value_,20.) && app.from_unit_==0);
    open_editor(app);
    app.update(side()); // +20 -> -20 Celsius
    focus_save(app); assert(close(app.input_value_,20.));
    app.update(m5()); assert(app.screen_==Screen::Conversion);
    assert(close(app.input_value_,-20.) && close(app.base_,253.15));
    choose_unit(app,2,2); // Negative Celsius -> Kelvin result valid.
    assert(app.to_unit_==2 && close(app.input_value_,-20.));
    // Switching to Kelvin as the *source* at the same -20 is physically invalid.
    go_to(app,1); app.update(m5());
    for(unsigned i=0;i<3 && app.candidate_unit_!=2;++i)app.update(side());
    assert(app.candidate_unit_==2);
    app.update(m5());
    assert(app.screen_==Screen::Units && app.invalid_unit_);
    assert(app.from_unit_==0 && close(app.input_value_,-20.));
    assert(close(app.base_,253.15));
    app.update(back()); assert(app.screen_==Screen::Conversion && !app.invalid_unit_);

    // Invalid -1020 °C cannot replace the confirmed -20 °C.
    open_editor(app);
    app.update(m5());   // Skip sign, select thousands.
    app.update(side()); // -1020
    assert(close(app.editor_.value(),-1020.));
    for(unsigned i=1;i<app.editor_.positions();++i)app.update(m5());
    assert(app.save_focus_);
    app.update(m5());
    assert(app.screen_==Screen::Editing && app.invalid_temperature_);
    assert(!app.save_focus_ && close(app.base_,253.15));
    assert(close(app.input_value_,-20.) && close(app.editor_.value(),-1020.));
    app.redraw(); assert(app.invalid_temperature_);
    app.update(back()); assert(app.screen_==Screen::Conversion);
    assert(!app.invalid_temperature_ && close(app.input_value_,-20.));
    open_editor(app); assert(close(app.editor_.value(),-20.));

    app.end();
    assert(app.update(m5())==Result::ExitRequested);
    select_category(app,3);
    assert(close(app.input_value_,1.) && app.from_unit_==1);
    assert(board.bounds_ok);
    assert(board.glyph_pixel_min_x>=0 && board.glyph_pixel_max_x<240);
    return 0;
}
