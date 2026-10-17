#!/usr/bin/env python3
"""Static rendering contract for two visual-only refinements."""
from pathlib import Path
root=Path(__file__).resolve().parents[2]
coin=(root/"apps/coin_flip/coin_flip_app.cpp").read_text(encoding="utf-8")
converter=(root/"apps/converter/converter_app.cpp").read_text(encoding="utf-8")
assert "board_.fill_circle(cx, cy, radius" in coin
assert "board_.fill_circle(cx, cy, radius - 3" in coin
assert "board::DisplayColor::Attention" in coin
assert "if (half < 28) return;" in coin
assert 'font_.draw("ZŁ"' in coin
assert "esp_random()" in coin
assert "state_.tick(now)" in coin
assert "font_.set_size(font_core::Size::Readable)" in converter
assert "font_.measure(text, 216, 24).fits" in converter
assert "draw_value(line,29" in converter
assert "draw_value(text,59+row*27" in converter
assert "cursor_width,4,board::DisplayColor::Accent" in converter
assert "const char focused[]" in converter
assert "editor_.character_index()" in converter
assert "editor_.increment()" in converter
assert "to_base(category_,unit_,editor_.value()" in converter
assert "ZA DUŻA WARTOŚĆ" in converter
assert "PONIŻEJ ZERA ABS." in converter
print("Coin/Converter visual-only source contract PASS")
