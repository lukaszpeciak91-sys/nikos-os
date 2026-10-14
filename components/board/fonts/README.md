# Polish UI font subset

Nikoś OS bundles a small generated subset of **DejaVu Sans** for Polish UI text.

Source:

- DejaVu Fonts project: https://dejavu-fonts.github.io/
- source typeface: `DejaVuSans.ttf`
- generated bitmap size: 9 px
- upstream license: Bitstream Vera font license; DejaVu additions are public domain

Bundled core coverage is intentionally limited to:

- ASCII `U+0020..U+007E`
- `Ą Ć Ę Ł Ń Ó Ś Ź Ż`
- `ą ć ę ł ń ó ś ź ż`

Facts v0.1 keeps its approved UTF-8 strings unchanged and uses the same scoped
Board font path. The dataset also contains a small number of foreign accented
letters and typographic punctuation not present in the original Polish subset.
Those exact codepoints are mapped to readable existing base glyphs in the font
metadata (for example accented Latin letters to their base letter and typographic
dashes/quotes to the ASCII dash/quote). This adds no general Unicode framework
and no duplicate bitmap payload.

The firmware does **not** bundle the original TTF. It contains only the generated 1-bit glyph subset used by the board rendering layer.

The subset contains 113 original bitmap-backed glyphs plus 15 Facts-only alias
glyph entries and 602 bytes of bitmap payload. With M5GFX glyph/range metadata, the expected raw font resource footprint on ESP32 is approximately **2.0 KiB** before normal linker/section overhead.

This is deliberately limited Polish UI support for Nikoś OS / Communicator v0.1. It is not a general Unicode, font-loading, localization-asset, or text-layout framework.

The deterministic board sanity/demo path renders exactly:

`ĄĆĘŁŃÓŚŹŻ ąćęłńóśźż CZEŚĆ! MOŻESZ GADAĆ?`

See `LICENSE-DejaVu.txt` for the bundled font license notice.
