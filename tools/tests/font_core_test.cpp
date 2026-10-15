#include "font_core/font_core.hpp"
#include <cassert>
#include <cstdint>
#include <cstring>
#include <cstdio>
#include <initializer_list>
#include "../../components/font_core/glyph_assets.hpp"
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
        assert(renderer.draw(u8"ABC €", {2, 2, 90, 40}).supported == false);
        assert(renderer.draw(invalid, {2, 2, 90, 40}).supported == false);
        for (const char* word : {u8"łl", u8"ła", u8"łó", u8"małe"}) {
            const auto tested = renderer.measure(word, 100, 40);
            assert(tested.fits && tested.supported);
            assert(renderer.draw(word, {0, 0, 100, 40}).fits);
        }
        assert(renderer.draw("OK", {0,0,10,19}).fits == false);
        assert(renderer.draw("OK", {0,0,40,18}).fits == false);
    }
    // Independently validate the dot vs acute upper-case raster.
    const nikos::font_core::detail::GlyphAsset* acute = nullptr;
    const nikos::font_core::detail::GlyphAsset* dot = nullptr;
    for (const auto& glyph : nikos::font_core::detail::kGlyphAssets) {
        if (glyph.codepoint == 0x0179) acute = &glyph;
        if (glyph.codepoint == 0x017B) dot = &glyph;
    }
    assert(acute && dot && std::strcmp(acute->rows, dot->rows) != 0);
    // Both glyphs retain the identical Latin Z body after the first row.
    assert(std::strcmp(acute->rows + 3, dot->rows + 3) == 0);
    // The dot must be centred independently of the acute marker.
    assert(std::strncmp(acute->rows, "100", 3) == 0);
    assert(std::strncmp(dot->rows, "040", 3) == 0);
    // No two Polish glyphs may silently share an identical raster.
    for (std::size_t i = 95; i < nikos::font_core::detail::kGlyphCount; ++i) {
        for (std::size_t j = i + 1; j < nikos::font_core::detail::kGlyphCount; ++j) {
            assert(std::strcmp(nikos::font_core::detail::kGlyphAssets[i].rows,
                               nikos::font_core::detail::kGlyphAssets[j].rows) != 0);
        }
    }
    // The composed l-stroke is confined to x offsets 1..3, below the
    // lowercase base l advance (4 px). The following character starts at 5.
    renderer.select(Mode::Composed);
    board.glyph_pixel_max_x = -1;
    assert(renderer.draw(u8"ł", {0, 0, 30, 19}).fits);
    assert(board.glyph_pixel_max_x < renderer.advance(0x0142));
    const int last_pixel = board.pixels;
    assert(!renderer.draw(u8"małe", {225, 130, 30, 19}).fits);
    assert(board.pixels == last_pixel);
    assert(board.bounds_ok);
    std::puts("Font core host tests passed");
}
