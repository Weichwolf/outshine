Type: bug
State: open
Area: ui
Tags: measured, hot-path, allocation

# No glyph is rastered inside a frame, because the sheet is baked before it

The text path is REACHED and it is CORRECT on the picture: `outshine-viewer --headless --show
apps/driver/src/f31.scenario --frames 3` sets "CORPUS", "CASE (1423)", the case list and
"1423 CASES" in three faces at two sizes. That is what moved `Typeface`, `Markup`, `Stylesheet`,
`Layout`, `Painting` and `Pointer` off the stranded side of CURRENT this round.

**What it does inside the draw is still forbidden.** `Glyph Typeface::Shape(...) const`
(src/ui/Typeface.h:31) resolves a cache miss by rasterising, and a miss costs two heap objects:

| src/ui/Typeface.cpp | what it does |
|---|---|
| `:185` | `SDL_Surface *ink = TTF_GetGlyphImage(set, (Uint32)code, &kind);` — allocates a surface |
| `:192` | `SDL_ConvertSurface(ink, SDL_PIXELFORMAT_RGBA32)` — allocates a second one whenever the face does not already hand back RGBA32 |
| `:212-213` | both are destroyed again, inside the same call |
| `:135` | `TTF_SetFontSize(set, (float)sizePx)` — SDL3_ttf FLUSHES the face's own glyph cache on a size change, so two sizes alternating in one frame re-raster each other's glyphs |

The faces open once and the sheet no longer grows — those two are fixed and measured
(`Opened()` is 3 for any run, `Missed()` counts exhaustion). What remains is the third: the
raster itself is lazy and it sits on the draw.

The house rule is not "amortised": *nothing inside the per-frame loops may allocate, lock, touch
disk, or grow a container — capacity is opened once, up front.* A first frame that pays 400 heap
allocations is a first frame that misses its 16.67 ms, and the p99 the engine is judged on is
exactly where that lands.

## What will be true

- [ ] The sheet is BAKED from a declared repertoire before the first frame: the markup a
      scenario declares names the code points, the stylesheet names the (family, size) pairs,
      and `Opens` cuts every cell of that product.
- [ ] `Shape` is a pure lookup — `const`, `noexcept`, no `mutable`, no SDL call.
- [ ] A code point outside the baked repertoire answers notdef and increments `Missed()`; it
      does not raster.
- [ ] Proving case: a scenario declaring a surface, run for 60 frames with the process heap
      counter read at frame 1 and frame 60 — the delta is ZERO bytes. Negative control: put the
      lazy raster back and the same case reports one allocation per unseen glyph.
