Type: bug
State: active
Area: render
Tags: measured, places

# A building's UNDERSIDE is not a wall, and it is not drawn

**Benchmark** — Unreal: a `UStaticMesh` built from a footprint extrusion has its base capped or left
open, and either way the cap carries its own material section; nothing calls a downward face a wall.
RAGE: building shells are authored, and a bottom face that could be seen would be a modelling defect
caught in review. **They agree, so the matter is closed**: a downward-facing face is either absent or
it is its own surface. **Taking that**, because the split this tree uses today cannot express it.

MEASURED, at Rothenburg from `build/places/OldTown.png` and the engine's own instrument:

    buildings: wall triangles                    305 250
    buildings: wall normals standing upright     746 802   (81.6 %)
    buildings: wall normals facing DOWN          168 948   (18.4 %)

The split in `src/engine/Picturing.cpp` is `aloft > 3.0 * kSteepestRoof` -- a face within 60 degrees
of vertical is a ROOF and **everything else is a wall**. "Everything else" includes every face
pointing straight down, so the plinth's underside, the extrusion's base cap and any inverted face all
wear the wall material and are drawn.

This was invisible until board:2020 made ambient depend on the normal. A downward face now takes the
ground's bounce and NONE of the sky, so it renders in the bounce's hue -- and those faces are plainly
visible in the frame at Rothenburg, on the tower and along the terraces. Before that change every
normal got the same ambient and a wrong-facing face looked exactly like a right-facing one. **The
lighting change did not cause this; it made it legible.**

## What is not yet known, and must be measured before anything is written

1. **WHY a downward face is visible at all from 490 m up.** A base cap under a solid is unreachable
   by any ray from above. Either the buildings are not closed, or the plinth extends below terrain
   that has since dropped away, or some footprints are wound so the extrusion is inside out
2. **Whether 18.4 % is the plinth or something else.** The number is the whole downward population
   and nothing yet separates base caps from inverted side faces. A count that cannot tell them apart
   cannot choose the fix

## The measurements that would show I am wrong

- **Split the count by cause**: base caps (the lowest ring of the extrusion) counted apart from every
  other downward face. If the second bucket is zero, the fix is to stop emitting caps and nothing
  else is wrong. If it is not zero, there are inverted faces and the winding is still not right
- **The negative control is a closed solid**: one footprint, extruded, rendered from above. Its base
  cap must contribute ZERO fragments. If it contributes any, the depth test or the winding is the
  finding and not the split
- **A cap that is removed must not open a hole.** Count fragments where the sky is visible THROUGH a
  building after the change; it must stay at zero

## The soup is the disease, and welding is not the cure

**Benchmark** — Unreal: a static mesh is an INDEXED mesh. The build welds by position and then SPLITS
render vertices where a normal or a UV differs, so the topology stays shared while the shading does
not. `FMeshDescription` carries vertices, edges and triangles as three separate things for exactly
this reason. RAGE: the same at export. **They agree, so the matter is closed**: the topology is one
thing, the render attributes are another, and a shared edge is shared whatever the two faces beside
it want to shade like.

MEASURED at Rothenburg: **2 055 586 of 2 511 000 emitted vertices are exact duplicates** — 82 per
cent. The generator pushes 8 floats per triangle corner into a flat soup and nothing in the tree ever
relates one corner to another. Closedness is therefore not a property the generator HAS; it is
something a walk reconstructs afterwards by welding on a centimetre grid, and that walk lives in a
test rather than in the engine.

That is why every defect this item has closed took a bisect to find. Five builders each wrote their
own polyline along one footprint edge, and nothing could notice they disagreed until a walk welded
them and counted. With a shared topology none of those five defects could have existed.

**Welding the soup is NOT the answer** and the reason is exact: a position where a wall meets a roof
legitimately carries two different normals and two different UVs, so welding by position alone
destroys the shading. That is what the soup is FOR. The answer is the split Unreal and RAGE both
make — positions and faces welded, render attributes per corner, derived from the topology at the
end rather than instead of it.

- [ ] `Site` builds a welded position table and an index buffer, and the soup is DERIVED from it
- [ ] the generator answers `Closed()` itself, so a hole is a refusal at build time rather than a
      number a test finds later
- [ ] `PushTri`'s degeneracy test becomes unnecessary: two corners at one position are one index
