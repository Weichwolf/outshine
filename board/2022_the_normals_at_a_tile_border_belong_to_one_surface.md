Type: bug
State: open
Parent: 2017
Area: world, render
Tags: measured

# The normals at a tile border belong to one surface

**Benchmark** — Unreal: landscape normals are computed across component boundaries from the shared heightmap, so a seam has one normal. RAGE: the same, authored. **They agree**, so the matter is closed: a point on the ground has ONE normal, whichever tile draws it.

MEASURED: the tiles' heights agree at the seam to 0.00 m over 4 832 shared vertices, and the tile
edges are STILL visible as a brightness checkerboard in all five places. Height is therefore not the
cause and the normal is the remaining candidate -- each tile computes its normals from its own
height grid, and a border vertex has no neighbour outside the tile, so the two tiles compute
different normals for the same point.

**THE OLD TREE KNEW THIS AND WROTE IT DOWN.** At `60dda039` the terrain shader carries the note:

    Without the gate, steep/aliased normals (more numerous on coarse LOD tiles) catch the
    below-horizon sun via (0.4+3*diff) -> a bright brightness-step at LOD seams.

So a brightness step at a seam was a known failure mode with a known cause, and the fix there was a
daylight gate on the sun term plus a mip bias taken from the VIEW RAY rather than from the surface
normal, because the surface normal mis-reads on camera-facing slopes.

## What will be true

- [ ] a vertex two tiles share carries one normal, whichever tile drew it
- [ ] the brightness step at a tile border is gone from all five places

## The measurements that would show I am wrong

1. **The angle, not the impression.** Co-located vertices are already hashed by east/north for the height check; the same pass takes the angle between their normals. If that reads near 0 deg while the checkerboard persists, the normal is NOT the cause and this item is misfiled
2. **The negative control is the seam itself.** With one normal per shared point, the widest disagreement must go to 0 deg; it cannot be declared fixed while that number stands above the raster's own slope quantisation
