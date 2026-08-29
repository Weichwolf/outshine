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
