Type: bug
State: open
Area: ui
Tags: measured, hot-path, allocation

# No glyph is rastered inside a frame, because the sheet is baked before it

**Benchmark** — Unreal: Slate bakes a font atlas and caches glyphs outside the frame. RAGE: Scaleform keeps a glyph cache. **Both agree** — rasterising a glyph inside a frame is an unbounded term on the frame path.

The text path is REACHED and it is CORRECT on the picture: `outshine-viewer --headless --show
apps/driver/src/f31.scenario --frames 3` sets "CORPUS", "CASE (1423)", the case list and
"1423 CASES" in three faces at two sizes. That is what moved `Typeface`, `Markup`, `Stylesheet`,
`Layout`, `Painting` and `Pointer` off the stranded side of CURRENT this round.

**What it does inside the draw is still forbidden, and one thing more since f6de15bf.**
`Glyph Typeface::Shape(...) const` (src/ui/Typeface.h:30) resolves a cache miss by rasterising:

| src/ui/Typeface.cpp | what it does |
|---|---|
| `:192` | `SDL_Surface *ink = TTF_GetGlyphImage(set, (Uint32)code, &kind);` — allocates a surface |
| `:200` | `SDL_ConvertSurface(ink, SDL_PIXELFORMAT_RGBA32)` — allocates a second one whenever the face does not already hand back RGBA32 |
| `:219-220` | both are destroyed again, inside the same call |
| `:133-146` | `TTF_Font *Typeface::Set(Family, int) const` — a `const` accessor on the draw path that opens a NEW `TTF_Font` over the held bytes on first sight of a (family, size) pair and `Sets_.push_back`es it into a `mutable` vector |

The size flush is fixed and measured: a face is read once with `SDL_LoadFile` and each
(family, size) gets its own instance through `TTF_OpenFontIO`, so no `TTF_SetFontSize` touches a
shared cache and no `Shape` call reaches the filesystem. What f6de15bf traded for it is a font
INSTANTIATION inside the first frame instead of a cache flush inside every frame — cheaper in
the mean, and still an allocation and an SDL call the house rule does not allow at all.

The house rule is not "amortised": *nothing inside the per-frame loops may allocate, lock, touch
disk, or grow a container — capacity is opened once, up front.* A first frame that pays 400 heap
allocations is a first frame that misses its 16.67 ms, and the p99 the engine is judged on is
exactly where that lands.

## What will be true

- [ ] The sheet is BAKED from a declared repertoire before the first frame: the markup a
      scenario declares names the code points, the stylesheet names the (family, size) pairs,
      and `Opens` cuts every cell of that product.
- [ ] `Shape` is a pure lookup — `const`, `noexcept`, no `mutable`, no SDL call — and so is
      `Set`: every (family, size) the stylesheet names is opened by `Opens`, before the frame.
- [ ] A code point outside the baked repertoire answers notdef and increments `Missed()`; it
      does not raster.
- [ ] Proving case: a scenario declaring a surface, run for 60 frames with the process heap
      counter read at frame 1 and frame 60 — the delta is ZERO bytes. Negative control: put the
      lazy raster back and the same case reports one allocation per unseen glyph.
