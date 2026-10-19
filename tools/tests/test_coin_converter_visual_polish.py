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
assert "draw_value(line,29,board::DisplayColor::PrimaryText,true)" in converter
assert "draw_value(text,59+row*27,board::DisplayColor::SecondaryText,false)" in converter
assert "cursor_width,4,board::DisplayColor::Accent" in converter
assert "const char focused[]" in converter
assert "editor_.character_index()" in converter
assert "editor_.increment()" in converter
assert "to_base(category_,unit_,editor_.value()" in converter
assert "ZA DUŻA WARTOŚĆ" in converter
assert "PONIŻEJ ZERA ABS." in converter
print("Coin/Converter visual-only source contract PASS")

board_h=(root/"components/board/include/board/board.hpp").read_text(encoding="utf-8")
board_cpp=(root/"components/board/board.cpp").read_text(encoding="utf-8")
assert "CoinGold," in board_h
assert "kCoinGoldRgb565 = 0xFDC8" in board_cpp
assert "return noir ? palette.accent : kCoinGoldRgb565;" in board_cpp
assert "case DisplayColor::Attention:" in board_cpp  # Existing warning stays unchanged
assert "board_.fill_rect(12, 27, 216, 29, background);" in converter
assert "board_.fill_rect(12, 27, 3, 29, board::DisplayColor::Accent);" in converter
assert "const std::int16_t x = primary ? 20 : 12;" in converter
assert "font_.measure(text, width, 24).fits" in converter
assert "font_.set_size(font_core::Size::Readable);" in converter
assert "font_.set_size(font_core::Size::Compact);" in converter
assert "draw_value(text,59+row*27,board::DisplayColor::SecondaryText,false);" in converter
assert "draw_value(line,29,board::DisplayColor::PrimaryText,true);" in converter
