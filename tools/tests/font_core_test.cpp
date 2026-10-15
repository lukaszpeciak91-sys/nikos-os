#include "font_core/font_core.hpp"
#include <cassert>
#include <cstdint>
#include <cstring>
#include <cstdio>
#include <initializer_list>
using namespace nikos::font_core;
int main() {
    nikos::board::Board board;
    Renderer renderer(board);
    assert(renderer.mode() == Mode::Standard);
    assert(renderer.metrics().letter_spacing == 0);
    assert(renderer.advance('A') == 6);
    assert(renderer.draw("ABC", {1,1,100,20}).fits);
    assert(board.text_calls == 1);
    assert(renderer.draw("ABCDEFGHI", {1,1,15,20}).fits == false);
    const int previous_pixels = board.pixels;
    assert(!renderer.draw("TEXT", {230,125,20,20}).fits);
    assert(board.pixels == previous_pixels);
    constexpr const char* polish =
        u8"ĄĆĘŁŃÓŚŹŻ ąćęłńóśźż";
    for (auto mode : {Mode::Composed, Mode::PreRendered}) {
        renderer.select(mode);
        const auto metrics = renderer.metrics();
        assert(metrics.line_height >= metrics.glyph_height);
        assert(metrics.letter_spacing >= 1);
        assert(renderer.advance('i') < renderer.advance('W'));
        assert(renderer.advance(0x0142) > 0);
        const auto fit = renderer.measure(polish, 220, 90);
        assert(fit.fits && fit.supported && fit.lines > 0);
        assert(renderer.draw(polish, {3,25,220,90}).fits);
        assert(board.bounds_ok);
        const auto multi = renderer.measure("Hello world again", 45, 100);
        assert(multi.fits && multi.lines >= 2);
        assert(renderer.draw("Hello world again", {0,0,45,100}).fits);
        assert(!renderer.measure("Supercalifragilisticexpialidocious", 20, 100).fits);
        assert(!renderer.measure("First\nSecond", 60, 19).fits);
        assert(renderer.measure("First\nSecond", 60, 45).fits);
        const char invalid[] = {'A', char(0xC0), char(0xAF), 'B', 0};
        const auto invalid_result = renderer.measure(invalid, 80, 40);
        assert(invalid_result.fits && !invalid_result.supported);
        const auto unknown = renderer.measure(u8"ABC €", 90, 40);
        assert(unknown.fits && !unknown.supported);
        assert(renderer.draw("OK", {0,0,10,19}).fits == false);
        assert(renderer.draw("OK", {0,0,40,18}).fits == false);
    }
    assert(board.bounds_ok);
    std::puts("Font core host tests passed");
}
