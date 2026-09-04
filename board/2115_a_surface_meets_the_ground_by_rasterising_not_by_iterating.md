Type: debt
State: active
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

## The order it lands in, decided 2026-09-04 after reading the renderer and the ring

Read before writing: the ring is `kBlockTiles` = 4 x 4 tiles per level, a level per doubling of
the sight (Kaiserberg 7 levels, ~100 tiles); the CPU samples every tile through 65 x 65 NODES
(`kStreamGrid` = 64, `FillNodeHeights`) and the drawn mesh is a 33-grid chunk cooked into a
DAG per tile on the pool; the classes are ALREADY decided in the fragment shader from
`ClassStructure::Words()` with `in.uv` = (east, north) in the anchor's tangent frame; a
piece's row is `[R_enu | t]` and the eye shift is added on the GPU. SDL_GPU has no mesh
shader, so the lattice is Unreal's: a shared grid mesh, INSTANCED per tile, the vertex
shader fetching the height from a texture (Landscape's vertex-texture fetch).

1. **Height pages.** The pool's `TileBuild` carries the tile's 65 x 65 nodes beside the chunk,
   made by the same `FillNodeHeights` the CPU query reads -- ONE grid for physics and picture,
   so a seat and the drawn ground agree to the float. The renderer holds them as pages in one
   `R32_FLOAT` 2D-array texture with a free list (17 KB a page; the ring is ~100 pages), placed
   and released by tile like a piece. Measured: bytes on the device, upload ms per page
2. **The lattice stage** (`Stage::Ground`, raster, fused into the subjects' pass, the same
   `SUBJECT_SURFACE` bindings and `fsGroundLit`): one 65 x 65 grid mesh with a SKIRT ring
   (Cesium's answer to the crack between two levels; CDLOD's morph is the Unreal refinement,
   added later if the skirt is SEEN), an instance per ring tile: row, the tile's four corner
   offsets in its own ENU (bilinear inside the tile keeps Mercator's north spacing to mm),
   page index, spacing. The vertex: height fetched, e/n from the corners, up = h - (e² + n²)/2R
   (the sphere's sag, Outerra's clipmap form; 6 cm at a 50 km tile), normal by central
   differences on the page, uv = the frame's east/north. A depth-only twin casts the shadow
   through `LightVisibilityStage`. Measured: the ring's triangles and ms in `Spent_`, and the
   nine pictures against their references with `pixels.py` -- the difference is the
   refinement's own (33-grid + refine/cut/sew) against the 65-node lattice, looked at
3. **Selection by the ring** as today (`LayPatchwork`'s per-level walk decides which tile at
   which zoom stands), the instances rebuilt when the ring moves, which is a list of ~100 rows
   and not a mesh. A tile crossing then uploads ONE page
4. **Deletion**: `Refine`, `Cut`, `Sew`, `Press`, `Classify`'s per-vertex walk,
   `DividesAtClassEdges`, the chunk cook and the ring's vertex upload; the road stays a
   ribbon, the buildings pieces, water a lattice at its own level. The four `ground: of that`
   measures read 0.000 ms and `ground-yield` leaves the heap
5. **Stamps (board:2121) on the worker, into the NODES**, not into the texture: the footprint
   and the corridor are rasterised into a tile's 65 x 65 grid where the tile is made, and the
   page uploaded stamped. One rule in one place, and the CPU query reads the same stamped
   nodes -- a compute pass would put the rule on the GPU where the headless path cannot read
   it back. Deviation from the paragraph above, with this reason
6. **Physics** reads what it reads today (`GroundStream::At` over the nodes), stamped

**What will show this was wrong**: a crack SEEN at a level boundary the skirt does not hide;
a seat that disagrees with the drawn ground by more than the float's quantum; a frame at
Kaiserberg slower than today's 3.4 ms p50 with the ring drawn by the lattice.

## Landed 2026-09-04: the lattice stands BESIDE the ring, declared, off by default

`Render.GroundLattice` (grammar child `groundLattice`, written back, `shots --lattice`) draws the
ground as `GroundLattice`: one 34 x 34 grid with a skirt, instanced per ring tile, the height
fetched from an `R32_FLOAT` page array (512 pages, 4.6 KB each), the tile's four corner offsets
bilinear inside the tile, the sphere's sag on the GPU, the class decided by the same
`fsGroundLit`. The page holds the nodes the patchwork's own chunk is built from (`kPatchGrid + 1`,
asserted), so the lattice IS today's ring surface before `Refine/Cut/Sew/Press`; the CPU ring
stays for the drape and the physics while the two are measured beside each other.

```
  OldTown, --lattice   5f8ca8d5   p50 2.18 ms   100 pages, 100 tiles, 0.07 M triangles
  OldTown, ring        303146af   p50 2.22 ms   -- the reference, unmoved by default
  pixels.py, lattice against the reference   30 019 differ, 11 244 by more than 1 of 255
    WHERE the ring was PRESSED: a road pixel (590, 590) is ground because the corridor's cut is
    in the ring and not in the nodes; a gable at (310..390, 510..560 of the near crop) has the
    ground through it because the pad's stamp is in the ring and not in the nodes; and the far
    band (rows 240..360) by the normal, central differences on the page against the mesh's
```

Found on the way: `SubjectDraw::Configure` runs TWICE per place (the plan recompiles once the
scene is declared), and a stage that rebuilds its residency there loses what was placed --
the residency now survives a reconfigure the way `SubjectResidency` does, and only the
pipelines are rebuilt.

**What decides the switch**: the stamps written into the NODES on the worker (step 5), and
the near field refined past the DEM's posting -- a footprint's rim at 36 m texels is not a rim,
Unreal's Landscape is authored at a metre, so the levels nearer than the finest tile are
VIRTUAL pages (the DEM upsampled, the stamps rasterised at that level's texel). Then the
pictures are looked at, and the references move with the owner's word.

## Landed 2026-09-04, step 5 at the DEM's own texel: the stamps press the NODES

The ring's `Press` rule now presses the sheets' nodes (`PressPoints`, the same `PressesAt` over
the same buckets) with the SAME subset the triangle budget takes (`Yielded::TakenWhich`) --
measured first with every yield and the nodes rose where the ring had not moved, 24 m at one
node, because `YieldGround` presses only what the budget took. The grid's vertices stand at
the chunk's own posting fractions (`ChunkNodePosting`), handed to the lattice once per stride.

```
  OldTown, --lattice   7c23fd78   pixels.py against the reference: 25 376 differ,
                                  7 275 by more than 1 of 255, 593 by more than 40 of 255
    593 strong: 317 in ONE cluster at (480..640, 480..600) -- a row house whose footprint is
    smaller than the 36 m node spacing, so no node falls inside it and the pad's stamp has
    nowhere to land; the ring had refined vertices INSIDE the footprint. The rest of the 7 275
    is the far band (rows 240..360): the normal by central differences against the mesh's
```

The number says what the next step is: the near field needs texels finer than the DEM's
posting -- VIRTUAL levels nearer than the finest tile, the page upsampled from the DEM and the
stamps rasterised at that level's texel, which is what Unreal's metre-grid Landscape has and
a 30 m DEM has not.

## Landed 2026-09-04, the near field VIRTUAL: four levels past the finest tile, 8 x 8 blocks

`HeightSheets::Refine` mirrors the ring's block rule downward: at zoom z+1 .. z+4 an 8 x 8 block of
tiles around the eye, each finer block replacing the centre 4 x 4 of the coarser, the nodes sampled
bilinearly from the SOURCE tile's stitched field (`GroundStream::FieldOf`, once per parent) -- the
DEM's own 4.8 m posting at z+3, and z+4 (2.3 m texels) already an upsample. A virtual sheet's grid
is uniform (a second grid buffer beside the posting-fraction one; two instanced draws), and the
stamps press its nodes like any other. Every pad presses the lattice (the ring's triangle budget
took ONE pad of 12 874 at OldTown -- the reference stands on unstamped pads), the corridors as the
budget took them, until board:2121 bounds a fill by its class.

```
                           ring      lattice   tiles  nodes pressed  pixels >1/255  >40/255
  OldTown   p50 ms         2.22      2.96      304    29 442         4 411          310
  Kaiserberg p50 ms        3.43      4.28      308    11 941         3 787           80
```

Three things the pictures said, each measured to its cause:

- **the lattice cast a shadow the ring never did** (317 of 593 strong pixels at OldTown: a
  terrain shadow on a gable roof). The ring's ground rows never cast (`CastsBelow`), so the
  lattice casts only under the same rule; whether terrain SHOULD shadow a house is board:2128's
- **the diagonal**: the lattice split each quad the other way from `ChunkQuadWinding`; on a saddle
  the two surfaces differ by metres. Matched -- 9 479 -> 6 105 pixels over 1/255 at one stroke
- **what remains is the SEAT**: a house stands where the coarse query (the stream's 65 nodes at
  z-1, a 36 m chord) put it, and the fine field is metres above that chord on a slope, so the true
  ground passes through the house's eaves. The pad's stamp cuts inside the footprint and its
  apron, not the hillside beside the wall. The answer is step 6: the query reads the SAME field
  the lattice draws (`PostingM` on the stitched source, not the 65-node tile), which moves every
  seat -- and every reference -- to where the ground actually is

The frame cost was the instance count, every tile drawn whether in view or not; the frustum
now decides, below.

## Landed 2026-09-04, step 6 under the flag: the seats and the roads read the FIELD

With `Render.GroundLattice` on, the bake's `HeightField` copies the finest source tile's
stitched field itself (`HeightField::CopiesField`, 256 x 256 postings, `TileHeightAslM` with
side == postings is the field's own bilinear) instead of the stream's 65-node block, and the
roads drape through `Drape::Field` -- `HeightSheets::FieldUpM`, the same `PostingM` the virtual
sheets sample, cached per tile -- instead of the ring's BVH. Seats and roads now stand on the
surface the lattice draws to the centimetre (bilinear against the triangulated 2.3 m nodes).

```
                       lattice+field   pixels >1/255   >40/255   p50 ms
  OldTown   4e7837d7                   22 995          10 632    2.92     (ring 2.22)
  Kaiserberg b0b40a3c                   8 399           1 025    4.36     (ring 3.43)
```

Looked at: the two pictures read as the references do -- the same town, the same roads, the
same fields -- and every strong pixel is a building or a road standing where the FINE ground
is rather than where the 36 m chord was; that is the move the switch will make deliberately.
One cluster stays unexplained: 132 strong pixels beside a row house at OldTown (480..640,
480..600), a hillside 8 m above the reference's road cut, present with 0 virtual levels, with
no skirt and with the chunk's diagonal -- the next thing to measure is that house's seat and
the corridor's cut at its wall.

Found on the way: the first lattice run at a place fetches the finest tiles' fields through the
stream's own cache (5 stitch grids, `kGroundStitchGrids`) and missed the 15 s patience cold;
warm, the preload holds. The field cache belongs beside the pool's byte cache, once.

**What the switch needs from here**: the owner's eye on the nine lattice pictures against the
references, then the lattice becomes the default, the ring's Refine/Cut/Sew/Press and its
vertex upload go (step 4), and nine new references are written with this item's word.

## The nine, lattice against reference, measured 2026-09-04 (pixels.py; the pictures in the item's commit)

```
  place        lattice digest  differ    >1/255    p50 ms (ring)
  OldTown      4e7837d7        47 488    22 995    2.91 (2.22)
  Heidelberg   0e755a78       307 403   142 706    3.21 (2.57)
  Shibuya      3b0df398        74 359    55 633    7.25 (6.81)
  CentralPark  e8d1c6e6       414 416   225 139    5.85 (4.76)
  Venice       a6fb124b       112 609    81 636    3.67 (2.74)
  Jura         c54b6613       156 030    25 340    3.77 (2.84)
  ZurichPlan   85111fff       163 348    25 082    6.22 (4.43)
  Kaiserberg   b0b40a3c        88 860     8 399    4.34 (3.43)
  Koehlbrand   ebba37d2        91 312    19 067    4.41 (2.84)
```

Looked at, all nine: the same places. What differs and why -- the ground's SHADING over
whole slopes (CentralPark's 45 per cent is the park's lawn a few units darker: the normal is a
central difference on the page, the ring's was averaged over its triangles), buildings and
roads standing on the fine ground instead of the chord, and at Heidelberg the near hillside
darker for the same normal reason. What has to be right BEFORE the switch: the normal
(the page's halo, then measured against the ring's on one slope), the cold preload (the
field cache), and the OldTown wedge; the frame cost is the frustum's now.

## Landed 2026-09-04: the frustum decides which tiles draw, and what a moved digest under the lattice means

Unreal's Landscape culls a component by its bounds against the view frustum on the CPU;
CDLOD selects nodes by the frustum during its quadtree walk. Here: every tile carries its
height range from its own nodes (`GroundTile::LowM` is the lowest node less the skirt's
drop and the sphere's sag, `HighM` the highest), the renderer keeps a sphere per instance
from that range and the tile's corners, and `GroundLattice::Cull` tests the four side planes
of the frame's MVP against every sphere before the frame's copy passes, writing the survivors
-- real first, then virtual, in their held order -- into a cycled buffer the lit pass draws.
The shadow pass still casts every tile: a caster outside the camera's frustum shadows what
is inside it. A first try with a constant 6 km slack passed 260 of 292 tiles; the tile's own
range passes 60 to 70.

```
  place        instances  drawn   p50 before  p50 after  ring
  OldTown      292        ~60     2.90        2.30       2.22
  Kaiserberg   300        ~70     4.43        3.65       3.43
  Jura         292        ~65     3.72        3.16       2.84
```

**The digests moved and the reason was measured, not assumed.** Every culled tile's height box
projects off screen (checked corner by corner), yet 57 pixels at OldTown changed by 1..7 of
255, 42 of them within one pixel of a projected tile edge (7 of 57 for random pixels in the
same band). The experiments, each one run of `shots --lattice OldTown`:

```
  nothing culled, through the new buffer                 digest as before
  nothing culled, forty sunk instances APPENDED           unchanged
  nothing culled, the real draw split into two draws      unchanged
  nothing culled, one sunk instance PREPENDED             moved
  nothing culled, the instances REVERSED                  moved
  depth compare GREATER -> GREATER_OR_EQUAL               unchanged  (no depth ties)
  no skirts / a constant normal                           still moved
  ONE tile alone, drawn as instance 0                     A
  the same tile as instance 1 via base instance, alone    A
  the same tile after a degenerate (zero-area) instance   B
  the same, the degenerate one in its OWN draw call       A
```

No overlapping tiles, no duplicates (checked). So on this GPU an instance's rasterization
depends on how many instances the SAME draw call processed before it, by an ulp, at
edge-on far pixels -- a property of the instanced draw, deterministic for a given stream
(three runs, one digest). The ring never showed it because it is one mesh in one draw whose
stream never changes. Under the lattice a digest that moves by a few pixels of a few units at
tile edges in the far band is this and nothing else; anything larger, or anywhere else, is a
change. That is the reading rule for every lattice digest from here on, and it is why the
cull's own digests (OldTown 32e0b780, Kaiserberg 9c6b446e, Jura 14daf15a) stand.

Found on the way: the normal at a page's rim was a one-sided difference, so two neighbours
disagreed about the normal along their shared edge -- a shading seam on every tile border.
Landed below.

## Landed 2026-09-05: the page carries a one-node HALO, and the stitched field is held once

Unreal's Landscape heightmap carries a one-texel border from the neighbouring component, so
the normal is a central difference on both sides of a seam and the seam is not there; CDLOD
and Cesium's terrain compute normals over the neighbour's data for the same reason. Here: a
page is `kPageSide = kSide + 2` nodes a side; the grid's vertex (i, j) reads texel (i + 1,
j + 1) and the normal is `(h[i+1] - h[i-1]) / 2 step` everywhere, rim included. The engine
builds the halo (`HeightSheets::Halos`): a real sheet keeps the pool's 34 x 34 nodes bit for
bit and gets its rim from the neighbours' stitched fields at the mirrored posting fraction
(`NodeFraction`: -f(1) and 2 - f(32), which is the neighbour's own node wherever the walk is
symmetric); a virtual sheet is sampled whole from its parent's field and the parent's
neighbours through one `AslAt(zoom, fx, fy)`. The stamps press the halo like any node, so
a stamp that crosses a tile edge presses both sides to the same value.

The halo made the field cost visible: 300 sheets asked for 284 stitched fields, each a stitch
of up to 13 raw grids through a FIVE-slot decoded cache -- 9 400 PNG decodes and 1.86 s at
Kaiserberg, and 3 s more waiting than before the halo. Two caches by BYTES, Cesium's rule for
a decoded-tile cache: the stitch pool's raw cache holds 8 MB [SET, ~30 fields of 264 KB,
three rows of an 8-wide block walk], and the stream keeps STITCHED fields once, shared
(`GroundStream::StitchedField` -> `shared_ptr<const TerrainField>`, 16 MB [SET]); the engine
borrows them instead of copying (`HeightSheets::Fields_`) and forgets them after the drape.

```
                        before halo   halo, 5 slots   halo, byte caches
  Kaiserberg waited     8.5 s         13.3 s          5.8 s
  Kaiserberg haloing    --            1 860 ms        612 ms
  Kaiserberg peak heap  343 MB        343 MB          296 MB
  Shibuya default heap  574..577 MB   --              585..589 MB   (the raw cache's 8 MB; board:2104's)
  Shibuya lattice heap  --            645 MB          595 MB
  Shibuya lattice wait  --            13.9 s          9.9 s
```

Digests (lattice): OldTown dcdb6248, Jura 9bf77900, Kaiserberg f1436e28, Heidelberg f04e740b;
the caches moved none of them. Against the cull's pictures the halo moved OldTown by 1 985
pixels, 260 over 1/255, worst 7 -- looked at: thin LINES along the tile edges in the far band,
the seam's other half, and nothing else; Jura 13 699 and 1 975, the same lines plus the
hillside's shading where the rim normal now has two sides. Against the references (ring) the
hillside at Heidelberg shades smoothly where the ring was faceted: the central difference is
the reference's own normal, so the comparison on a slope is settled by construction and not by
a number. The nine references unmoved by default.

Named and still open: single dark pixels on Jura's plain (150 of 255, the skirt of a finer
tile seen through a crack at a level boundary where the coarser edge's chord lies below the
finer node -- Unreal stitches the finer edge to the coarser one, Cesium hides it with the
skirt and a texture; this tree's skirt is lit and shows) and the OldTown wedge.
