Type: debt
State: open
Area: base, generators
Tags: architecture, owner

# The geometry a generator needs is a LIBRARY, and no generator writes its own

**Benchmark** -- Unreal: `FGeomTools2D`, `FPoly`, `FDynamicMesh3` and `GeometryProcessing` are one
library every tool and every plugin uses; a landscape spline and a procedural-mesh actor call the
same triangulation. RAGE: the geometry primitives sit beside the maths in `rage` and every module
takes them from there. **Neither lets a subsystem carry its own polygon code**, for the reason this
tree can already measure: two copies drift, and only one of them gets the bug fixed.

## The duplication is measured, not suspected

| primitive | where it stands today |
|---|---|
| segment intersection | `base/spatial/Wayfinding.cpp`, `compositor/GroundYield.cpp` |
| ring area, point in ring | `world/ground/BuildingField.cpp`, `world/ground/WaterField.cpp`, `generators/draw/BuildingShape.cpp` |
| triangulation | `import/Subject.cpp`, `generators/draw/RoofSurface.cpp` |
| half-plane side | `generators/draw/BuildingShape.cpp` |
| mesh soundness | was TWICE: `Laying.cpp`'s census and `BuildingMesh`'s `Judged`, and the second was reached by nothing |

The census is the proof of the rule. It was written once inside an engine file where no generator
could see it, so a second one grew in `BuildingMesh` -- and that second one was dead, so nobody
ever knew whether it agreed with the first.

## What the library holds, and the owner's rule

"outshine braucht wie math/ eigene geometrie bearbeitungs und modelierungs klassen, die von
generatoren verwendet werden müssen. nicht jeder generator darf das rad neu erfinden." The OSM
generator uses no exotic algorithm; it uses the ordinary ones badly placed.

| | verbs |
|---|---|
| **polygon** | signed area, orientation, point inside, convexity, offset, clip by half-plane, simplify, triangulate |
| **polyline** | resample at a step, arc and chord fit, offset to a ribbon, segment intersection, trim at a meeting |
| **mesh** | refine (`Divide`), weld, census, recompute normals, stitch a seam, PRESS to a profile |
| **field** | drape: the height of a soup at a place; the spatial index behind it |

Four of these already stand where they belong, moved this round: `base/spatial/Refine.h`,
`base/spatial/Census.h`, `base/spatial/Drape.h`, `base/geo/PlaceKey.h`. The rest is the work.

**Pressing is the one that matters most** and it is half-written: `YieldGround` presses the ring to
a set of `Yields`, and the other half is the levelling loops still inside `Paves`. A rail, a runway,
a canal and a building pad all want that verb, and today only a road can say it.

## What will be true

- [ ] Each primitive above stands ONCE, under `src/base/`, reachable by every generator
- [ ] No `src/generators/**` or `src/world/**` file carries its own copy of one, and a case walks
      the tree to say so the way `TheEngineNamesNoSubject` walks it for subject nouns
- [ ] Each carries a proof with a VENDOR oracle where one exists -- a triangulation against a
      known-good result, a polygon area against a computed one -- rather than agreement with
      ourselves
- [ ] The generators' door offers them, so a client writing its own generator reaches the same
      library rather than writing a fifth ring-area

## Why this is not a sweep

A shared primitive that is subtly different from the copy it replaces moves every picture. Each
one lands on its own, with the nine places quoted before and after, and the digests are the proof
that the two agreed.
