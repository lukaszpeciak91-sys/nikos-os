#!/usr/bin/env python3
"""Static rendering contract for two visual-only refinements."""
from pathlib import Path
root=Path(__file__).resolve().parents[2]
coin=(root/"apps/coin_flip/coin_flip_app.cpp").read_text(encoding="utf-8")
converter=(root/"apps/converter/converter_app.cpp").read_text(encoding="utf-8")
assert "board_.fill_circle(cx, cy, radius" in coin
assert "board_.fill_circle(cx, cy, radius - 3" in coin
assert "board::DisplayColor::CoinGold" in coin
assert "board::DisplayColor::Attention" not in coin
assert "if (half < 28) return;" in coin
# The denomination now spells ZŁOTY and its glyph bounds are checked
# against the real pre-rendered raster in test_coin_flip_visual.py.
assert 'constexpr const char* label = "ZŁOTY";' in coin
assert 'font_.measure(label' in coin
assert 'font_.draw(label' in coin
assert "esp_random()" in coin
assert "state_.tick(now)" in coin
assert "font_.set_size(font_core::Size::Readable)" in converter
assert "focus_box(12, 22, 216, 26, focus_ == Focus::Value)" in converter
assert "focus_box(12, 49, 103, 24, focus_ == Focus::From)" in converter
assert "focus_box(120, 49, 108, 24, focus_ == Focus::To)" in converter
assert '"WARTOŚĆ"' in converter and '"WYNIK"' in converter
assert "draw_fitted(result_text, 17, 94, 210" in converter
assert "from_base(category_, to_unit_, base_, result)" in converter
assert "font_.measure(text, w, 24)" in converter
assert "font_.measure(text, w, 19)" in converter
assert "font_.set_size(font_core::Size::Compact)" in converter
assert "const char focused[]" not in converter
assert "font_.advance(" in converter
assert "editor_.character_index()" in converter
assert "save_focus_ ? board::DisplayColor::Surface" in converter
assert '"ZAPISZ"' in converter
assert "board::DisplayColor::Accent" in converter
assert '"PONIŻEJ ZERA ABS."' in converter
print("Coin/Converter v0.2 visual styling and single-result layout PASS")

board_h=(root/"components/board/include/board/board.hpp").read_text(encoding="utf-8")
board_cpp=(root/"components/board/board.cpp").read_text(encoding="utf-8")
assert "CoinGold," in board_h
assert "kCoinGoldRgb565 = 0xFDC8" in board_cpp
assert "return noir ? palette.accent : kCoinGoldRgb565;" in board_cpp
assert "case DisplayColor::Attention:" in board_cpp
