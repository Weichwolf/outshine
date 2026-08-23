Type: bug
Area: ground
Tags: tiles, seam, currency

**The mesh reads its heights in the currency it places them**

Found while closing 1746: the tile layer carries TWO fraction currencies over one field.

- `PostingFrac(k, n) = k/(n-1)` (TerrainGrid.h) -- a POSTING lattice: fraction 0 is the
  first posting, fraction 1 the last. Positions are placed with it.
- `TexelIndex(frac, n) = frac*n - 0.5` (TileMath.h) -- a TEXEL grid: fractions address
  cell centres. `InterpolatedM` samples with it.

`TerrainMesh::Over` (TerrainGrid.cpp:46-56) mixes them in ONE vertex: the position comes
from `PostingFrac(r, rowsOut)` and the height from `field.InterpolatedM(fc, fr)` with the
same fractions. Every interior vertex therefore takes the height of a place half a posting
spacing away from where it is placed (at the extremes the Bilinear clamp hides it, which is
why no picture screams). At Terrarium's ~30 m spacing at z12 that is a systematic ~15 m
horizontal misregistration of the terrain against its own heights -- and it grows with
stride.

1746 repaired the stitcher by giving `TerrainField` a second reader, `PostingM`, in the
posting currency, and the seam closed exactly (187.5 m of gap -> 0). The same reading is
what `Over` owes its vertices.

What will be true: `Over` samples heights with `PostingM`; any caller that means texels
says so at the call site; a unit twin (beside AStitchedEdgePairsPostingsOfTheSamePlace)
proves a linear-ramp field meshes to vertices whose heights equal the ramp AT THEIR OWN
positions, which the texel reading cannot satisfy.
