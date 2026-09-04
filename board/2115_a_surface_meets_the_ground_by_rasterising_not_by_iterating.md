Type: debt
State: open
Area: generators, base, engine
Tags: architecture, performance, owner
Supersedes: 2097

# The ground is a HEIGHT FIELD the GPU tessellates, and nothing is refined, cut or sewn

**Benchmark** -- Unreal's Landscape is a regular grid per component displaced by a heightmap,
LOD as lattice level, the seams between components a KNOWN, enumerated case; a spline or a
footprint is a breakline the edit layer honours. RAGE's terrain is gridded and baked, roads cut
in at cook time. **Neither searches for seams**, because neither creates them. What is mine is
the resolution rule, because the world arrives over the wire.

**Cited beside the two**: Cesium's terrain is `quantized-mesh` per tile -- a TIN with its edge
vertices listed so neighbours match, and a SKIRT down every tile edge so a crack between two
levels never shows -- and its `sampleHeightMostDetailed` is the ray down a tile that step 1 of
board:2121 already is. The lattice comes from Unreal; the seam discipline comes from Cesium.

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
- [ ] THE GEOMETRY IS GAP-FREE AND SNAPPED: `CensusOver` the composed ring -- ground, stamps,
      corridors, water -- reads 0 open edges besides the ring's outer rim and 0 overused edges,
      and every vertex two pieces share is ONE vertex (welded at the lattice's quantum, never
      two within an epsilon). This is a case, and it is the oracle every later change to the
      ground is held to
- [ ] `kDigestBasis == kFnv64Basis`, and the commit says every digest moved and why

## What will show I was wrong

A projected grid that still needs a seam pass. Then the construction was not coherent and this
item misread the problem.

## THE BETTER ANSWER, decided 2026-09-04: the lattice lives on the GPU, and the CDT shrinks to the kerb

The projected lattice above is right and the CPU is the wrong place for it. This target has mesh
shaders and a tile-based GPU; the reference terrains that stream a planet do the tessellation ON
the GPU from a height TEXTURE, and the CPU uploads heights, never vertices:

| | who | what |
|---|---|---|
| **CDLOD** | Strugar 2009; Unreal's Landscape does the same with vertex-texture fetch | a quadtree of fixed lattice patches; the vertex shader reads the height texture and MORPHS a vertex toward the coarser level as it nears the level boundary, so two neighbouring levels meet with no seam BY CONSTRUCTION and no skirt |
| **spherical clipmaps** | Clasen & Hege 2006; Outerra | the same over a sphere, for the 393 km horizon a flight needs |
| **stamps** | Unreal `FlattenHeightEditBrush` | a footprint or a corridor is RASTERISED into the height texture by a compute pass; the lattice then carries the slab and the grade wherever it is sampled, at every level |
| **classes** | this tree already | the class raster is sampled in the fragment shader (`groundClasses`, `groundPalette` exist); `Classify` per vertex on the CPU goes |
| **physics** | RAGE `phBoundHeightfield` | the SAME height texture, read on the CPU as a heightfield bound (board:2127) |

What that deletes: `Refine`, `Cut`, `Sew`, `Press`, `LayPatchwork`'s gather, `Classify`,
`DividesAtClassEdges`, the vertex upload of the ring per rebuild, and the CPU LOD selection that
duplicates the GPU DAG. What stays geometric: the ROAD stays a ribbon (a car drives on it, a kerb
has an edge), pressed by a corridor stamp under it (board:2121); the constrained edge the CDT was
for exists only at the kerb, and the ribbon IS that edge. Water is a lattice at its own level.

The measurement this is held to: `ground: of that, refining/cutting/sewing/pressing` read 0 ms;
`heap taken under ground-yield` and `ground-patchwork` fall to the height tiles' bytes; the
eight references move only where the lattice's level differs from today's refinement and the
pixels are looked at; and a tile crossing uploads ONE height tile, not a ring.

## Inherited from board:2122, closed 2026-09-04

The buildings are pieces in the renderer's pool, baked per vector tile on a worker. The
terrain tile is NOT yet a piece: the ground's classify and press are world-grained passes
(`ground-yield` took 830 MB of heap at Shibuya, `Grounds(true)` adds 83 MB live), and a tile
can only become a piece once its surface is produced per tile -- which is this item.
