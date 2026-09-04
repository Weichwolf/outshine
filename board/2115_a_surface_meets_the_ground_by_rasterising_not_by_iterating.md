Type: debt
State: open
Area: generators, base, engine
Tags: architecture, performance, owner
Supersedes: 2097
Depends: 2122

# The ground is a PROJECTED LATTICE with REQUIRED edges, and nothing is refined, cut or sewn

**Benchmark** -- Unreal's Landscape is a regular grid per component displaced by a heightmap,
LOD as lattice level, the seams between components a KNOWN, enumerated case; a spline or a
footprint is a breakline the edit layer honours. RAGE's terrain is gridded and baked, roads cut
in at cook time. **Neither searches for seams**, because neither creates them. What is mine is
the resolution rule, because the world arrives over the wire.

## Where it stands, measured 2026-09-04

```
  generators/terrain/GroundYield.cpp
    Refine   :400   split the longest edge until WantedEdgeM, kMostPasses = 6, a FlatMap per pass
    Cut      :534   find where an outline crosses a face, kCutPasses = 5
    Sew      :637   stitch the halves, kSewPasses = 6, a fresh CellGrid per pass
    Press    :814   move the ring to the yields
  Kaiserberg, one rebuild   refine 94 + cut 93 + sew 81 + press 42 + count 12 = 322 of 1051 ms
  heap taken under ground-yield                                     2 787 MB over four rebuilds
```

Three passes that are one problem stated backwards: the mesh is built irregular and then
REPAIRED into coherence. The primitive `Divide` is right; the LOOP around it is the finding --
and the loop does not even use the primitive, it uses its own copy (`LayCutFace`, board:2103).

## The solution, named after the reference was looked up

Two halves, and the first version of this item had only one of them.

**A projected grid settles RESOLUTION.** A regular lattice is projected onto the field and
displaced by the height; neighbours share vertices because they are neighbours in the lattice,
so there is no seam to find. Resolution is the lattice's own level, INDEXED from the tile rung
rather than approached by halving. This is an ENGINE verb, beside the shadow: *project a lattice
onto this field at this resolution*, and a generator asks for it -- terrain, water, and anything
else with a surface to cover. Two callers before it is written is what makes it real.

**A CONSTRAINED DELAUNAY TRIANGULATION settles the EDGES.** A footprint or a kerb is a
BREAKLINE, and the structure that carries one is a CDT: the lattice's points are the input
points, the outline is a CONSTRAINT and therefore an edge, and there is no seam to sew. ArcGIS
states the property this item is about -- "no densification occurs, and each breakline segment
is added as a single edge" -- and PostGIS ships it as `ST_TriangulatePolygon`. Refine/Cut/Sew is
the detour taken when an edge cannot be REQUIRED. Requiring it is the primitive this tree lacks,
and it lands in `base/geometry/` (board:2103) with a vendor oracle: a triangulation of a known
polygon set against a known-good result.

| here | with a lattice and a CDT |
|---|---|
| grid, then `Refine` until the edge is approached | the lattice's level is the resolution |
| `Cut` where an outline crosses a triangle | the outline is a constraint edge |
| `Sew` the halves back | nothing was cut |
| `Press` | the lattice is displaced by the yields at construction (board:2121) |

**And the digest basis is corrected in the same commit.** Every picture moves under a
structural rewrite, so the guard is `test/scripts/pixels.py` against `build/shots/reference/`
and not the digest -- which is the one day the FNV-1a basis can be corrected for free:
`kDigestBasis` is the published constant with its last digit dropped (`Digest.h:13-25` asserts
both facts), and the commit that renumbers every digest anyway makes it the number its name
claims.

## What will be true

- [ ] `ground: of that, refining`, `cutting the seams`, `sewing them` read 0.000 ms because the
      passes no longer exist; `heap taken under ground-yield` falls by the order the arithmetic
      predicts
- [ ] The lattice is an engine verb with two callers, terrain and water
- [ ] The CDT stands in `base/geometry/` with a vendor oracle, and a footprint's outline is an
      edge of the ground mesh, a case counting the edges that coincide with it
- [ ] No pixel of the eight references differs by more than 1 of 255 unless the difference is
      looked at and named
- [ ] `kDigestBasis == kFnv64Basis`, and the commit says every digest moved and why

## What will show I was wrong

A projected grid that still needs a seam pass. Then the construction was not coherent and this
item misread the problem.
