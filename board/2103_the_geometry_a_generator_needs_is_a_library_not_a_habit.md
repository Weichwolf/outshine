Type: debt
State: open
Area: base, generators
Tags: architecture, owner
Supersedes: 2116

# The geometry a generator needs is a LIBRARY, and each primitive stands once

**Benchmark** -- Unreal: `FGeomTools2D`, `FPoly`, `FDynamicMesh3` and `GeometryProcessing` are
one library every tool and plugin uses; a landscape spline and a procedural-mesh actor call the
same triangulation. RAGE: the geometry primitives sit beside the maths in `rage` and every module
takes them from there. **Neither lets a subsystem carry its own polygon code**, because two copies
drift and only one gets the bug fixed.

## Where it stands, measured 2026-09-04

| primitive | copies | where |
|---|---|---|
| segment intersection | 2 | `base/spatial/Wayfinding.cpp:584`, `generators/terrain/GroundYield.cpp:453` (different epsilons) |
| ring area | 4 | `world/ground/BuildingField.cpp:101`, `generators/building/BuildingShape.cpp:177`, `world/ground/WaterField.cpp:213`, `import/Tangents.cpp:252` |
| point in ring | 3 | `BuildingField.cpp:218`, `generators/building/RoofSurface.cpp:84`, `generators/base/FeatureField.cpp:82` |
| ear-clip triangulation | 2 | `RoofSurface.cpp:48`, `WaterField.cpp:213` |
| red-green split | 2 | `base/spatial/Refine.h::Divide`, `GroundYield.cpp:354 LayCutFace` -- byte for byte, with `kNoVertex` redeclared |
| undirected edge key | 2 | `Refine.h::EdgeKey`, `GroundYield.cpp:53 EdgeKey(EastSouth, EastSouth)` -- a different meaning under the same name |
| a millisecond clock | 3 | `ClassStructure.cpp:27`, `ClassField.cpp:32`, `ClassBuilder.cpp:36` |
| half-plane side | 1 | `BuildingShape.cpp:72` |

And the word `Refine` names five things: `Refine.h` (a file named for a caller), `GroundYield::Refine`
(the adaptive loop), `RoofSurface::Refine` (dome subdivision), `Laying::RefineChords` (stations
along a curve), `BuildingMesh::Refined` (ring densification). A sweep over the word is
CLAUDE.md's four-meanings trap; the rename is per type with the compiler as the oracle.

Already out and standing once: `Refine.h::Divide` (the engine's `DividesAtClassEdges` uses it),
`Census.h`, `Drape.h`, `geo/PlaceKey.h`, `TriangleBvh`, `ClusterCook`.

## The solution

`src/base/geometry/` -- beside `base/math/` -- with the verbs and their vendor oracles:

| | verbs | oracle |
|---|---|---|
| **polygon** | signed area, orientation, point inside, convexity, offset, clip by half-plane, simplify, triangulate | area against a computed one; triangulation against a known-good result |
| **polyline** | resample at a step, arc and chord fit (`base/curve/` already), offset to a ribbon, segment intersection, trim at a meeting | intersection against exact rational arithmetic |
| **mesh** | `Divide` (the red-green split), weld, `CensusOver`, recompute normals, PRESS to a profile | a closed mesh's Euler characteristic |
| **field** | `Drape`, `TriangleBvh` | height at a point against a direct sample |

Each primitive lands on its own with the nine places quoted before and after, and the digests are
the proof the copies agreed. `LayCutFace` goes first because it is a byte-for-byte copy; the
four ring areas second. The header is named for what it holds: `Refine.h` becomes `Divide.h`.

## What will be true

- [ ] Each primitive above stands ONCE under `src/base/`, reachable by every generator, and the
      generators' door offers them
- [ ] A claim walks `src/generators/**` and `src/world/**` for a second copy the way
      `TheEngineNamesNoSubject` walks for nouns, and reads 0
- [ ] `grep -rn '\bRefine' src include` names one thing
- [ ] Each primitive carries a proof with a VENDOR oracle where one exists, never agreement with
      ourselves

## What will show I was wrong

A shared primitive that moves a picture on landing. Then the two copies disagreed, and which one
was right is looked at before either is kept.
