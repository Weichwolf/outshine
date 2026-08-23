Type: bug
Area: ground
Tags: ub, robustness, tiles

**A field too small to mesh refuses before the stitcher reads it**

`TerrainTiles::StitchedGrid` (src/ground/tiles/TerrainTiles.cpp:137-141) takes the field
out of `RawGrid` with `TryFieldMutable()`, which asks only whether the grid is *Decoded* --
never whether it is `Meshable()`. `StitchEdge` guards the NEIGHBOUR
(TerrainTiles.cpp:110 `if (!n || !n->Meshable())`) and does not guard `self`.

On a 1x1 field the currency division underflows:

    src/ground/tiles/TerrainGrid.h:57
      inline double PostingFrac(uint32_t k, uint32_t n) { return (double)k * (1.0 / (double)(n - 1u)); }

`n == 1` gives `0.0 * inf = NaN`, `PostingM` scales the NaN, and `Bilinear` casts it:

    src/ground/tiles/TileMath.h:23
      const uint32_t x0 = (uint32_t)gx, y0 = (uint32_t)gy;

Proven, UBSan, `-fno-sanitize-recover`:

    src/ground/tiles/TileMath.h:23:52: runtime error: nan is outside the range of
    representable values of type 'unsigned int'
    SUMMARY: UndefinedBehaviorSanitizer: undefined-behavior
    -> Abort trap: 6

Reachable from the wire: `TerrainGrid::FromTerrariumPng` (TerrainGrid.cpp:18) builds
`TerrainField(read.High, read.Wide)` from whatever the PNG declares, and a 1x1 PNG is a
valid PNG. `RawGrid`'s crop arm refuses a too-small crop (TerrainTiles.cpp:92
`if (cropCols < 2 || cropRows < 2)`) -- the UNCROPPED arm has no such check, so a
same-zoom 1x1 answer walks straight into the stitcher. `CacheStore` already knows the
predicate (`if (Cache_.empty() || !field.Meshable()) return`); the caller does not.

The layer runs without a sanitiser (see the reopened 1743), so the gate cannot see this.

What will be true:

1. `RawGrid` returns `TerrainGrid::NotHere()` for a decoded field that is not `Meshable()`
   -- the same verdict the crop arm already gives, in one place; the stitcher then cannot
   be handed one.
2. `PostingFrac` refuses `n < 2` by construction (a `static_assert`-able precondition or a
   guard that returns 0.0), so no future caller mints a NaN fraction.
3. `test/unit/ground/tiles/` carries the 1x1 and 1xN cases and passes under the sanitised
   arm.
