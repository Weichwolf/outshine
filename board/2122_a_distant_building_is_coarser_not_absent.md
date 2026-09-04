# A distant building is coarser, not absent

State: open

Shibuya is the one place that does not preload, and the reason has never been broken down before.
Measured 2026-09-04 with the ceiling temporarily lifted, then restored:

```
  world: the bytes its fields hold        856.7 MB
  world: the buildings                    668.9 MB    <- 78% of it
    of those, the footprints                50.7 MB
    and the RAISED GEOMETRY                618.2 MB    <- the meshed buildings
  world: of that, the land classes         59.6 MB
  world: the frame copies the renderer reads  735.2 MB <- a SECOND copy
  world: the streets                        3.6 MB
  world: the water                          0.02 MB
```

For comparison, OldTown holds 26.8 MB of buildings and 16.2 MB of frame copies. Shibuya is
twenty-five times that, against a 512 MB ceiling.

**SO IT IS NOT A LEAK AND NOT AN OVERHEAD: it is the geometry itself.** Every building in range is
meshed at full detail, and the frame copy doubles it. 1.4 GB standing for one place.

## The answer is the one the goal already names

HLOD, baked as Unreal and RAGE bake it: **when a building is meshed, BOTH the fine geometry and a
proxy are produced, and frame time only CHOOSES between them.** A distant building becomes
COARSER, never absent -- deleting it is what makes a skyline flicker, and both references refuse
that for the same reason.

What that changes here: `world: and the raised geometry` stops holding one mesh per building at
one detail. It holds the fine mesh for what is near and a proxy for what is far, and the proxy for
a city block is one box-like shell rather than thirty facades.

## What Unreal does, what RAGE does

Unreal bakes HLOD clusters offline into proxy meshes per cell, chosen by screen size at run time.
RAGE bakes LOD models per entity plus an even coarser SLOD per block. **They agree on the shape and
they agree on the timing** -- the proxy is built when the geometry is built, never in the frame.
Here the world arrives over the wire, so "offline" becomes "during preload", which is the same
substitution board:2110 already makes for generators.

## What will show I was wrong

`world: and the raised geometry` on Shibuya, against the 512 MB ceiling for the whole world. Today
618.2 MB for buildings alone. And the picture: a skyline that loses a building between two frames
means the proxy is missing rather than coarse, which is the failure mode this item exists to
prevent.

## Measured 2026-09-04: the peak is the BAKE's grain, not the geometry's size

The vertex went from 32 bytes to 20 -- the format every reference stores -- and the buildings fell
from 281 MB to 198. Shibuya stayed red at 541530534 bytes against 536870912. So the ceiling is not
about what the world weighs.

`BuildingField::Built_` is ONE `Raised` for every building of every tile. Nothing empties it;
`AddedFirst_` only remembers where the current tile's vertices begin. A std::vector doubles as it
grows, so at the moment it crosses a power of two the process holds one and a half times the whole
world's building geometry, and the peak is that instant rather than the resting size.

Measured at CentralPark: fields hold 281 MB, frame copies 168 MB -- 449 MB of a 933 MB peak. The
other 484 MB never appears in any held total because it is the bake's own working set. Allocation
that flowed through the tagged regions in one run: tile-worker 1.98 GB, ground-yield 967 MB,
world-ground 857 MB.

WHAT THE REFERENCES DO: bake per cell and hand the result on. RAGE builds a map section, ships it,
and reuses the buffer; Unreal cooks per level chunk. The peak is one cell, not the world, and the
resting size is what the GPU holds. Here the bake is world-grained on the CPU side and the ceiling
is checked against a total that was never meant to exist all at once.

So this is the next block, and it is one change rather than three: bake a tile, hand it over, reuse
the buffer. Peak, preload and the moving camera all sit on it.

## And what holds the buffer open, read rather than assumed

After `CarryIntoTheFrame` runs, `Models()` reads only `WallRun` and `RoofRun` off the built
geometry -- the INDICES. Positions and normals it takes from the frame copies, which are the arrays
the renderer is handed. So the 198 MB of vertices on the CPU side are not being read for anything
after the handover; they are held so the NEXT tile can be appended and carried incrementally
(`Already = World.WallCarried` skips what has already crossed).

That is the whole knot, and it names the fix: the mesher should write into the frame copy and there
should be one buffer, not a soup that is later transcribed into one. A vector cannot release its
front, so no amount of clearing after the fact reaches this -- the second buffer has to stop
existing rather than be freed sooner.

THE GRAIN IS ALREADY RIGHT: `Build()` works on `next.Tile` from `field.Tiles()`, which is a VECTOR
tile, not a terrain tile, and since the rung stopped bounding the detail decision the two ladders
are independent as they should be. What is world-grained is only the buffer.
