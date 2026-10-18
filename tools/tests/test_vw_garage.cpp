#include "vw_garage/catalogue.hpp"
#include "vw_garage/vw_garage_app.hpp"

#include <cassert>
#include <cstdio>
#include <cstring>

using nikos::vw_garage::VwGarageApp;
using nikos::vw_garage::kModels;
using nikos::vw_garage::kModelCount;
using nikos::board::InputState;

InputState m5_short() {
    InputState result{};
    result.primary_short = true;
    return result;
}
InputState side_short() {
    InputState result{};
    result.secondary_short = true;
    return result;
}
InputState side_long() {
    InputState result{};
    result.secondary_long = true;
    return result;
}

int main() {
    static_assert(kModelCount == 25, "Approved catalogue count");
    nikos::board::Board board;
    nikos::font_core::Renderer renderer(board);
    renderer.select(nikos::font_core::Mode::PreRendered);
    renderer.set_size(nikos::font_core::Size::Compact);
    auto check = [&](const char* label, int width, int height, int lines) {
        const auto measured = renderer.measure(label, width, height);
        assert(measured.fits && measured.supported);
        assert(measured.width_px <= width);
        assert(measured.height_px <= height);
        assert(measured.lines == lines);
    };
    check("GARAŻ VW", 156, 19, 1);
    for (unsigned i = 0; i < kModelCount; ++i) {
        assert(kModels[i].debut_year >= 1945);
        if (i > 0) assert(kModels[i].debut_year >= kModels[i-1].debut_year);
        check(kModels[i].name, 164, 19, 1);
        check(kModels[i].name, 216, 19, 1);
        check(kModels[i].vehicle_type, 94, 19, 1);
        check(kModels[i].description_pl, 216, 38, 2);
        assert(std::strchr(kModels[i].description_pl, '\n') != nullptr);
    }

    VwGarageApp app(board);
    app.begin();
    assert(board.bounds_ok && board.marker_y == 29);
    app.update(m5_short());  // First catalogue card.
    assert(board.bounds_ok && board.marker_y == -1);
    app.update(m5_short());  // Same model: list viewport unchanged.
    assert(board.marker_y == 29);
    for (unsigned i = 1; i <= 24; ++i) {
        app.update(side_short());
        assert(board.bounds_ok);
        assert(board.marker_y == 29 + 22 * (i < 4 ? i : 3));
    }
    app.update(m5_short());   // Last card (25/25).
    app.update(side_short()); // Wrap to first card (01/25).
    app.update(m5_short());   // Adjust viewport to show first model.
    assert(board.bounds_ok && board.marker_y == 29);
    app.update(m5_short());
    app.update(side_long());  // Card -> list, no application exit.
    assert(board.marker_y == 29);
    assert(app.update(side_long()) == VwGarageApp::UpdateResult::ExitRequested);
    app.end();
    assert(app.update(m5_short()) == VwGarageApp::UpdateResult::ExitRequested);
    app.begin();              // New admission resets selection.
    const int redraw_before = board.clear_calls;
    app.redraw();             // Overlay redraw must retain selection.
    assert(board.clear_calls == redraw_before + 1);
    assert(board.marker_y == 29 && board.bounds_ok);
    std::puts("VW Garage catalogue, font and navigation host tests PASS");
}
