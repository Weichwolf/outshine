Type: bug
State: open
Area: ui, render
Tags: hot-path, measured, allocation

# The glyph sheet is baked before the frame, and no frame reads a font off disk

`Typeface` rasters lazily from inside the draw. `Paint.cpp:115` and `Layout.cpp:411,483` call
`Face->Shape(code, sizePx, family)` per glyph per frame; `Shape` reaches `Cell0f`, and on a miss
that path does all three of the things the frame path may not do:

| src/ui/Typeface.cpp | what it does inside a frame |
|---|---|
| `:127  TTF_Font *set = TTF_OpenFont(path.c_str(), (float)sizePx);` | opens a 757 KB face **off disk** |
| `:146  Rgba_.resize((size_t)SheetW_ * (size_t)taller * 4u, 0u);` | **grows** the sheet, reallocating and copying |
| `:128,164,168,176,202  Sets_.emplace / Cells_.emplace` | **allocates** a node per glyph, into `std::map` |

`src/clients/Live.cpp:458` then re-uploads the whole sheet whenever `Cut()` moves, so a single
unseen glyph costs a disk read, a realloc and a full texture upload in the middle of a frame.

CLAUDE.md: *no alloc/lock/disk/unbounded block on the frame path; capacity is opened once, up
front.* The lazy cache is the wrong shape for this engine, not merely an unoptimised one — a
scenario declares its surfaces, so the set of (family, size) it can ask for is KNOWN at
`Opens()`.

The two `std::map`s are the second defect and independent of the first: a node-per-glyph tree
keyed on `uint64_t`, walked per glyph, is a pointer chase where a flat open-addressed table over
a contiguous array is the layout this tree asks for everywhere else.

## What will be true

- [ ] `Opens()` bakes the sheet for every (family, size) the standing scenario's surfaces
      declare, and reports how many cells and how many bytes it took.
- [ ] `Shape()` on the frame path performs NO open, NO resize and NO insert. An undeclared cell
      answers with the notdef cell and increments a published counter, so the gap is a number a
      scenario suite can assert on rather than a stall nobody sees.
- [ ] The cell table is contiguous and pointer-free; `std::map` leaves the file.
- [ ] Proving case: a scenario draws text for N frames and the count of disk opens after frame
      zero is 0; negative control — the bake removed, the count is non-zero and the case reds.
