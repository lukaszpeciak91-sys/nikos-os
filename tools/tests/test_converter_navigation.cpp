#include <cassert>
#include <cmath>
#include <cstdint>

#include "board/board.hpp"
#include "font_core/font_core.hpp"
#include "converter/conversion.hpp"
// Expose existing state only in the host regression; no firmware API changes.
#define private public
#include "converter/converter_app.hpp"
#undef private

using nikos::board::InputState;
using nikos::converter::Category;
using nikos::converter::ConverterApp;

InputState m5_short() {
    InputState input{};
    input.primary_short = true;
    return input;
}
InputState side_short() {
    InputState input{};
    input.secondary_short = true;
    return input;
}
InputState side_long() {
    InputState input{};
    input.secondary_long = true;
    return input;
}

bool close(double a, double b) {
    return std::fabs(a - b) < 1e-7;
}

int main() {
    nikos::board::Board board;
    ConverterApp app(board);
    using Screen = ConverterApp::Screen;
    using Result = ConverterApp::Result;

    app.begin();
    assert(app.screen_ == Screen::Categories && app.selection_ == 0);
    InputState power{};
    power.power_short = true;
    const int clears = board.clear_calls;
    assert(app.update(power) == Result::Running);
    assert(app.screen_ == Screen::Categories && board.clear_calls == clears);
    assert(app.update(side_long()) == Result::ExitRequested);

    // Full hierarchy: Categories -> Conversion -> Categories -> Tools.
    app.begin();
    app.update(side_short()); // Length category.
    assert(app.selection_ == 1);
    app.update(m5_short());
    assert(app.screen_ == Screen::Conversion);
    assert(app.category_ == Category::Length && app.unit_ == 0);
    assert(close(app.base_, 1.0));
    app.update(side_long());
    assert(app.screen_ == Screen::Categories && app.selection_ == 1);
    app.update(m5_short());
    assert(app.screen_ == Screen::Conversion && app.category_ == Category::Length);
    app.update(side_short()); // cm, preserving selection and confirmed value.
    assert(app.unit_ == 1 && close(app.base_, 1.0));
    app.update(m5_short());
    assert(app.screen_ == Screen::Editing && close(app.editor_.value(), 100.0));

    // Unconfirmed edits must survive redraw and suspend/resume, not cancel.
    app.update(side_short()); // Thousands place: 0100.0 -> 1100.0 cm.
    assert(close(app.editor_.value(), 1100.0));
    assert(close(app.base_, 1.0));
    const int before_suspend = board.clear_calls;
    app.suspend();
    assert(app.update(side_long()) == Result::Running);
    app.redraw();
    assert(board.clear_calls == before_suspend);
    assert(app.screen_ == Screen::Editing);
    app.resume();
    assert(board.clear_calls == before_suspend + 1);
    assert(app.screen_ == Screen::Editing);
    assert(close(app.editor_.value(), 1100.0));
    app.update(side_long()); // Cancel, not jump to Categories.
    assert(app.screen_ == Screen::Conversion);
    assert(app.category_ == Category::Length && app.unit_ == 1);
    assert(close(app.base_, 1.0));
    assert(!app.invalid_temperature_);
    assert(!close(app.editor_.value(), 1100.0));
    app.redraw();
    assert(app.screen_ == Screen::Conversion);
    assert(app.unit_ == 1 && close(app.base_, 1.0));

    // Reopening starts from the last confirmed quantity, not discarded digits.
    app.update(m5_short());
    assert(app.screen_ == Screen::Editing);
    assert(close(app.editor_.value(), 100.0));
    app.update(side_short()); // 1100 cm = 11 m.
    for (int i = 0; i < 5; ++i) app.update(m5_short());
    assert(app.screen_ == Screen::Conversion);
    assert(close(app.base_, 11.0));
    assert(app.category_ == Category::Length && app.unit_ == 1);

    // Cancel another changed value after one successful commit.
    app.update(m5_short());
    assert(close(app.editor_.value(), 1100.0));
    app.update(side_short());
    assert(!close(app.editor_.value(), 1100.0));
    app.update(side_long());
    assert(app.screen_ == Screen::Conversion && close(app.base_, 11.0));
    assert(app.unit_ == 1);
    app.update(side_short());
    assert(app.unit_ == 2 && close(app.base_, 11.0));
    app.update(side_long());
    assert(app.screen_ == Screen::Categories && app.selection_ == 1);
    assert(app.update(side_long()) == Result::ExitRequested);

    // Invalid temperature: cancel clears the error without saving bad digits.
    app.begin();
    app.update(m5_short()); // Temperature conversion, 20 °C.
    assert(app.category_ == Category::Temperature && app.unit_ == 0);
    assert(close(app.base_, 293.15));
    app.update(m5_short());
    assert(app.screen_ == Screen::Editing && close(app.editor_.value(), 20.0));
    app.update(side_short()); // + -> -
    app.update(m5_short());   // Move to thousands.
    app.update(side_short()); // -1020.0 °C: below absolute zero.
    assert(close(app.editor_.value(), -1020.0));
    for (int i = 0; i < 5; ++i) app.update(m5_short());
    assert(app.screen_ == Screen::Editing && app.invalid_temperature_);
    assert(close(app.base_, 293.15));
    app.redraw();
    assert(app.invalid_temperature_);
    app.update(side_long());
    assert(app.screen_ == Screen::Conversion);
    assert(!app.invalid_temperature_);
    assert(close(app.base_, 293.15));
    assert(app.unit_ == 0 && app.category_ == Category::Temperature);
    app.update(m5_short());
    assert(app.screen_ == Screen::Editing);
    assert(close(app.editor_.value(), 20.0) && !app.editor_.negative);
    for (int i = 0; i < 6; ++i) app.update(m5_short());
    assert(app.screen_ == Screen::Conversion && close(app.base_, 293.15));
    app.update(side_long());
    assert(app.screen_ == Screen::Categories);
    assert(app.update(side_long()) == Result::ExitRequested);

    app.end();
    assert(app.update(m5_short()) == Result::ExitRequested);
    app.begin();
    assert(app.screen_ == Screen::Categories && app.selection_ == 0);
    assert(board.bounds_ok);
    return 0;
}
