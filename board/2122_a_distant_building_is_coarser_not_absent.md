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
