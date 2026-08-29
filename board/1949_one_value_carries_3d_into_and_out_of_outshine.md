Type: feature
State: active
Parent: 1953
Progress: door
Area: door, content, generators
Tags: benchmark, target, owner

# ONE value carries 3D data into and out of outshine, and glTF is one of its file forms


## THE FRAMING THIS ITEM CARRIED IS WITHDRAWN: glTF IS AN IMPORT PATH, NOT THE INTERNAL VALUE

Written into this item earlier: "our standard asset format is glTF and generators are dynamic glTF
suppliers -- outshine is a glTF assembler". **That is wrong, and the measurement is what refutes
it.** `Gltf::Subject` as the value everything crosses on is precisely what forces the reshaping
between the producer and the device:

    Site soup, 8 floats interleaved  ->  Gltf::Subject  ->  PackVertices de-interleaves  ->  upload

2 708 ms of pure copying on Shibuya, and 3 374 MB moved for a world holding about 900 MB. Not a slow
copy -- a copy that a decision created.

**THE CORRECTED SHAPE.** glTF gets an IMPORT PATH like any other producer, and every producer --
importer and generator alike -- delivers data in the form the RENDERER binds, preferably in SDL3's
own types:

    glTF importer  --\
    OSM, terrain,     >--  the layout the device binds  -->  device
    vegetation     --/

**Benchmark** — Unreal: the glTF importer is an EDITOR path that produces the same cooked buffers a
native asset produces; nothing at runtime carries a glTF shape. RAGE: every importer lands in
`grmGeometry`, and the file format it came from is gone by then. **They agree, and neither carries an
interchange format inward.** Taking that.

This does not weaken the item's title -- ONE value still carries 3D in and out. It corrects WHICH
value: not the one a file happens to use, but the one the device reads. A value that has to be
re-laid on the way is more than one, and glTF's layout is not the device's.

## WHAT THE CORRECTION DELETES, and every line carries what it costs today

The framing withdrawn above was holding up machinery that has no other reason to exist. Measured on
Shibuya's rebuild this round:

| goes | because | measured today |
|---|---|---|
| `PackVertices` | the producer already writes the device's layout | 2 708 ms and a 900 MB copy |
| `Assemble`, Geometry -> Gltf::Subject | there is no second internal form to reach | 2 437 ms |
| `SubjectScratch` | nothing left to flatten INTO | the target of that copy |
| 3.7x the bytes moved | each reshaping wrote its own | 3 374 MB for ~900 MB of data |
| `SubjectResidency`'s staging ring | `SDL_MapGPUTransferBuffer`'s `cycle` is the mechanism | the owner's SDL3 rule |

**AND IT UNBLOCKS board:1992.** That item is not untouched work -- it is stopped on exactly this,
in its own words: "`Cook` takes an `outshine::Geometry` and the whole subject path carries a
`Gltf::Subject` ... the two values have to become one first". With glTF as an import path rather
than the inward language there is one value, and the cooker has something to cook.

**The order, smallest and provable first**, because a rewrite that lands half-done is worse than the
copy it replaces:

1. the producers' vertex layout is DECLARED once and matches what the pipeline binds -- a written
   layout with a `static_assert`, before a byte moves
2. `PackVertices` becomes a no-op for producers that already agree, and the case that shows it is
   the rebuild's own phase clock falling to zero there
3. the glTF importer writes that layout instead of `Gltf::Subject`, which is where the interchange
   format stops travelling inward
4. `Assemble` and `SubjectScratch` go when nothing reads them -- and the audit that finds an
   unreached capability is the one that says so

## THE OWNER'S RULE, and it decides the layout question this item has been circling

**A generator must deliver data the rest of the pipeline does not have to change.** Not "deliver it
efficiently" -- deliver it in the form the device binds, so that nothing between the two reshapes
it. A copy that exists only because two stages disagree about a layout is not a cost to optimise,
it is a decision nobody made.

MEASURED, and the chain is four representations of one thing:

    Site soup (8 floats interleaved: position, uv, normal)
      -> Geometry parts
      -> Gltf::Subject
      -> PackVertices, which DE-INTERLEAVES into separate runs
      -> the residency's upload

`PackVertices` exists for exactly one reason: the generator writes interleaved and the renderer
binds separate streams. On Shibuya that disagreement costs 2 708 ms of pure copying, and the upload
that follows carries 900 MB that were already in memory in a valid form. Under the rule the answer
is not a faster pack -- it is that the pack STOPS EXISTING, and the upload points at what the
generator wrote.

That is this item's own sentence read strictly: ONE value carries 3D in and out, and a value that
has to be re-laid on the way is more than one.


## THE HIP'S HOLES ARE A SCALE DEFECT, and two derivations died proving it

A square footprint with a hip roof opens, and the sweep now holds the cases that say so. Height,
pitch, use and eaves overhang are all ruled OUT by measurement -- towers of one height over rising
footprints, and one footprint over two heights:

    footprint   6     7     8  |   9    10    11  m
    d (ridge/2) 5.07  5.92  6.76|  7.61  8.45  9.30 mm
    triangles   70    70    70 |  64    64    64
    holes        0     0     0 |   6     6     6

Height is irrelevant: 6 m at 42 m and at 55 m are both clean, 11 m at 42 m and at 55 m both break.
The overhang is 0.42 m on every one of them, including the clean ones. The break is between a
footprint of 8 and 9 m and the bigger building has FEWER triangles, which is the signature of
triangles being DROPPED rather than never built.

**A square house, 9 by 9 m, was missing from the sweep and reproduces the tower exactly** -- 6 holes,
4 flat and 2 upright. It also reached `house/hip`, a seventh architecture the sweep had never built.

**TWO HYPOTHESES DIED HERE, and both were arithmetically clean:**

The first said the hip's two parallel diagonals are 10 to 20 mm apart in that band, so `Deduped`
calls them two lines while `ClipHalf` goes on snapping cuts within `kWeldM` onto the corner. The
predicted boundary -- `d * sqrt(2) = kSamePointM`, so d = 7.07 mm, a footprint of 8.37 m -- lands
within 4 per cent of the measured break between 8 and 9 m. **It is a coincidence.** Widening
`Deduped` to `kWeldM` changed NOTHING, and a probe at a tolerance of 1.0 m -- which merges five
crease lines into three -- produced byte-identical output: 64 triangles, 6 holes.

The second said the toolchain might not be rebuilding what I changed. A destructive probe answers
that: with `CreasesUncounted` returning ZERO creases for a hip, both cases drop to 44 triangles and
0 holes. The tree is honest and the creases are the cause.

**A THIRD HYPOTHESIS DIED TO THE COUNTER IT ASKED FOR.** Crease crossings are not it either:

    footprint      6    7    8  |   9   10   11
    breaks kept   24   24   24  |  24   24   24
    breaks dropped 96   96   96  | 144  144  144
    holes           0    0    0  |   6    6    6

The KEPT set is identical everywhere, and widening the dedupe took the broken cases from 144
dropped to 72 -- FEWER than the clean ones -- with the holes unmoved. More dropped crossings do not
mean holes, and the crossings that survive are the same crossings.

## FOUND, AND IT IS TWO THRESHOLDS INSIDE ONE FUNCTION

For a near-square plan `halfU - halfV = d`, so the hip line `{1,-1,d}` passes EXACTLY through two
opposite corners and its partner `{1,-1,-d}` passes `d * sqrt(2)` away from them -- 9.56 mm at an
8 m footprint, 10.76 mm at 9 m. `ClipHalf` asks "is this corner ON the line" with `kOnLineM`, which
was `kSamePointM` = 10 mm, and then asks "is this cut THE corner" with `kWeldM` = 20 mm, four lines
below in the same function. Between the two the corner is beside the line for the clip and on the
corner for the snap, the cell between them is cut out by one step and collapsed by the next, and its
neighbours keep the vertices it loses.

`kOnLineM = kWeldM` -- one question, one number. MEASURED, and the six triangles come back:

    case              before          after
    square house 9x9  76 tri 6 holes  82 tri 0 holes
    9, 10, 11 m tower 64 tri 6 holes  70 tri 0 holes
    broad spire 11x11 64 tri 6 holes  70 tri 0 holes
    terrace / long hall / slab        unchanged, as predicted

## WHAT THE REMAINING TWELVE EDGES ARE, located rather than counted

The instrument now reports each hole edge's length, the height of its ends, their distance from the
building's middle, and WHICH SURFACES meet there -- decoded from the soup's own U coordinate, where
`FaceUvX` writes `-(kind + 16 * ident)`. Two different defects, not one:

**Terrace and slab -- a T-junction inside the plinth.** Three edges of 4.090, 8.180 and 4.090 m,
and 4.090 + 4.090 = 8.180 EXACTLY: the three points are collinear and the missing triangle has zero
area. Both ends carry only `plinth`. The 8.18 m is the part's 8.00 m edge plus twice the 0.09 m
plinth proud, so it is the widened plinth ring, subdivided at the gable ridge crossing on one side
and whole on the other. The slab is the same shape at 1.185 + 0.374 = 1.559 m.

**The 16 m tower -- the pitch does not close against the trim.** Three edges of about 16.6 m running
from the ridge at 42.53 m down to the eaves at 30.95 m, `pitch | pitch+trim`, plus two 28 mm edges
along the eaves. A different seam in a different place, and its 12 overused edges belong to it.

**SIX HYPOTHESES HAVE NOW DIED ON THESE TWELVE EDGES**, every one of them measured rather than
stacked on the last: party-wall flags, crease deduplication, crease crossings, the hip diagonals'
spacing (whose predicted break landed within 4 per cent of the real one and was still the wrong
cause), the ear clip producing a degenerate ear at a collinear point (`EarClip` requires a strictly
positive area, so it never cuts one), and the ear clip FAILING outright -- `Fill` counts that in
`Unclipped_` and the count is 0 for every case in the sweep. The case had been reading that counter
only to clear it, which is the same as not having it.

**What the measure cannot see, and it is the judge's own turn to be measured.** `Judge` welds on a
CENTIMETRE grid while the mesher snaps on a MILLIMETRE one, and its key is an XOR of three products
with no position check behind it -- two distinct points can collide into one vertex and no line of
this instrument would notice. Before the last edges are chased, that has to be sound, or the thing
being chased may be the ruler.

## THE PARTY EDGE MUST NOT WIDEN, AND SAYING SO IS NOT ENOUGH

A terrace house has no eaves overhang into its neighbour and no plinth under their wall, and
`Widened` moved every vertex by one mitred distance -- so each part's roof was pushed THROUGH the
wall it shares and the two neighbours covered the same strip twice. `Widened` now takes a per-edge
mask and solves the corner exactly (`y.N0 = d0`, `y.N1 = d1`, a 2x2 solve that reduces to the old
mitre when the two offsets are equal; a 90 degree corner gives (byM, byM), checked by hand).

**MEASURED, and it is much worse rather than better:**

    case         before            after
    terrace       3 holes  8 over   3 holes  20 over
    long hall    10 holes  8 over  64 holes  24 over
    slab          3 holes 30 over 156 holes 115 over

Reverted. The geometry is right and the introduction is not: every builder BEHIND the offset --
the soffit, the trim, the covering -- subdivides against the breaks of the UNWIDENED ring and
assumes a uniform offset, so the moment the ring changes shape at a party corner they stop meeting
in the same points. Trading 46 overused edges for 229 holes is not a step toward a closed solid.

The per-edge `Widened` is kept because it is correct and its default is the old behaviour; what is
missing is that `BreaksBoth` and `RefinedLike` have to be told the same thing. That is the item's
"topology is OWNED" guarantee and it is one change, not two.

**AND KEEPING IT CLOSED THE LONG HALL, which nothing predicted.** The old guard read

    const double miter = byM / (0.5 * len * len);
    if (std::fabs(miter) > 4.0 * std::fabs(byM)) return {};

`miter` is a MULTIPLIER and `4 * byM` is a LENGTH -- the test compares two different units, and
where it fired wrongly `Widened` returned an EMPTY ring, the roof was silently built on the
unwidened one, and the surfaces beside it no longer met. The exact solve compares the offset's real
length instead. The long hall goes 10 hole edges to 0 and the sweep's total 22 to 12.

## AND IT UNCOVERED THE NEXT ONE RATHER THAN REMOVING SIX

A 16 m square tower was clean before because this defect never let the sweep reach it. It now reads
126 tri, **6 holes and 12 OVERUSED edges**, 2 flat and 4 upright, over a band 11.6 m tall -- where
the old defect was 4 flat and 2 upright in a 4 cm band. A different defect wearing the same count.
So the honest arithmetic is 6 closed and 6 uncovered, and the total stands at 22.

A 20 m and a 28 m square are `tower/flat` with no creases at all, which is why they are clean and
why they are not evidence about hips. They reached an EIGHTH architecture the sweep had never
built.

**Benchmark** — Unreal: `FMeshDescription` authored against `FStaticMeshLODResources` + `FNaniteResources` cooked. RAGE: `grmGeometry` cooked, and its file IS that form. **Both agree** — two forms and one cooker; this tree has nine.

Owner's target, arrived at over three exchanges and stated here whole:

- generators do not serialise; they hand back the internal representation (board:1948)
- a glTF serialiser ships beside them, for a caller who wants a file
- **that representation is the UNIVERSAL interface for exchanging 3D data with outshine**

The third sentence is the one that changes the shape. The value is not "what a generator returns"
-- it is what ANYTHING hands outshine and what outshine hands back. A glTF reader fills it. A
generator fills it. A foreign program fills it directly, with no file anywhere. The compositor
consumes it. The serialiser writes it out. One value, many producers, many consumers.

## COUNTED, and the benchmark line is exactly right: two forms, and this tree has eight

**Benchmark** — Unreal: `FMeshDescription` to author, `FStaticMeshRenderData` to draw, one cooker
between them. RAGE: `grmGeometry` cooked, and the file IS that form. **Both agree on TWO**, so the
number is the finding rather than the existence of a second one.

Every type in this tree that OWNS or VIEWS vertex data, with how many vertex-carrying fields each
declares:

    form                          fields   what it is for
    include/Geometry.h             10      the DOOR. Parts, spans, materials, lamps, transforms
    Gltf::Subject                  17      the owning form. Doubles, plus morphs and animation
    Patchwork                       5      the ground ring, already flattened and CPU-culled
    TileBuild                       1      one tile: Verts, Idx, Clusters
    BuildingField                   4      the OSM soup and its footprints
    SubjectScratch                  4      packed per frame for the upload
    ClusterDag                      1      the cooked cut
    Meshed                          0      a wrapper that fills a `Geometry` from a soup

Eight, and `Meshed` is the only one that is honestly a helper. The other seven each hold vertices in
their own layout, and geometry is copied between them on the way from a producer to a pixel.

**Two of them are the right two.** `Geometry` authors and something cooked draws. The other five are
the defect, and each one exists because a producer had nowhere to put its output: the ground built a
`Patchwork`, the tiles a `TileBuild`, the OSM a `BuildingField`, the renderer a `SubjectScratch`.
None of them could hand back a `Geometry` because `Geometry` could not carry what they hold -- which
is this item's own line, "`include/Geometry.h` must therefore carry whatever a FILE can carry, or a
generator is weaker than a file and the interchange claim is false."

## The value already exists, and it is named after a format

`Gltf::Subject` (`src/content/gltf/Subject.h`) owns positions, uv, uv1, normals, tangents,
colours, indices and parts, and assembles from `Gltf::Piece` -- a non-owning VIEW of the same
fields. That is exactly an interchange value: the view to hand data in, the owning form to hold
it.

Three things are wrong with it as it stands:

- **It is named `Gltf`.** A universal 3D value named after one of its file forms is a lie about
  what it is, and it will read as "glTF-specific" to every future author. glTF is a serialisation
  of this value, not its identity. CLAUDE.md's *glTF 2.0 is the only content surface* stays true
  and gets sharper: one surface, one in-memory value, and the format is how it lands on disk.
- **It is not public.** `include/` names it nowhere, so a client cannot hand outshine geometry
  without going through a file.
- **One file fills one from pieces.** `grep -rln 'Gltf::Piece'` over `src/` finds
  `src/engine/Engine.cpp` alone -- the ground ring. Every other producer goes through the reader.

## THE READER MUST FILL IT, AND THAT IS WHAT MAKES THE ASYMMETRY IMPOSSIBLE

Where else would the reader read TO? Today it reads into `Gltf::Subject` while the door carries
`Geometry` -- two representations of one thing, which is the second spelling of a truth this tree
forbids, and it is the whole reason the two can drift apart.

Measured at HEAD, the drift: 19 glTF extensions reach the picture, and the builder carries vertex
streams.

| the reader takes it from a file | the builder can hand it in |
|---|---|
| positions, normals, uv0, uv1, tangents, colours, indices | yes -- and PROVEN by the round trip for positions, normals, uv0 and colours |
| a per-part material INDEX | yes -- an int into a table that does not exist without a file |
| **the materials themselves** -- base colour, metallic, roughness, emissive, alpha mode, double-sided, every texture slot, and the nine `KHR_materials_*` the reader honours | **no** |
| **punctual lights** (`KHR_lights_punctual`) | **no** |
| node hierarchy and transforms | no -- parts arrive pre-baked into one space |
| skins, joints, inverse bind matrices | no |
| animations and morph targets (`Subject::Build` takes a pose and weights) | no |
| cameras | no |
| material variants (`KHR_materials_variants`) | no |

Materials are the sharpest row: they reach the renderer from the DOCUMENT
(`src/engine/Live.cpp:57` walks `file.Materials()`) and never from the subject, so geometry handed
through the door names a material index into nothing.

**Filling the gap row by row is the wrong repair.** Point the reader at the one value and the gap
cannot open: the builder holds everything the reader produces BY CONSTRUCTION, and every feature
the reader gains afterwards arrives on both sides at once. That is Unreal's shape -- the importer
produces `FMeshDescription` and the renderer builds its proxy FROM it -- and RAGE's, where the tool
chain produces one `rmcDrawable` and nothing else exists to drift from.

## THE PRECISION QUESTION HAS TWO ANSWERS BECAUSE THERE ARE TWO STAGES

Stated wrongly here first, and the correction is the useful part. The claim was that `Subject`'s
`double` positions merely widen the file's float32 and carry no information, because the reader
does not bake node transforms. **It does bake them**: `src/content/gltf/Subject.cpp:694` runs
`place.At(vertex).Point(local, global)` -- the node's world transform AND the skin -- in double,
and stores the result. So the doubles are the output of a double-precision chain, not a widening.

With that measured, the two precisions turn out to belong to two different stages:

| stage | what it holds | precision |
|---|---|---|
| **`Geometry`** -- what the file says | source vertices per part, model-local, **plus the node transform that places them** | `float`, which is glTF's own (accessor component type 5126) |
| **the packing** -- what is drawn | placed and skinned vertices in one array, parts as reaches | `double`, because the chain that produced them runs in double |

Two stages of one truth, which is not the same thing as two truths. Unreal draws the line in the
same place: `FMeshDescription` holds what was imported, `FStaticMeshRenderData` holds what is
drawn, and the transform precision lives in `FTransform` rather than in the vertices.

**This makes `Geometry` carry the node hierarchy**, which the table below already lists as absent --
so the correction removes a row rather than adding one. The merge runs: the reader fills a
`Geometry` from the file's primitives and transforms, and the placement bake derives the packing.

## What will be true

**MEASURED: NINE INDEPENDENT GEOMETRY REPRESENTATIONS STAND IN THE TREE**, where the benchmark
holds two.

| where | what it is |
|---|---|
| `include/Geometry.h` | the AUTHORED form — the one that is meant to be the only one |
| `content/gltf/Subject.h` | the reader's, `Positions_` in `double` |
| `render/SubjectProxy.h` | `SubjectScratch`, packed interleaved floats — the COOKED form |
| `base/spatial/ClusterDag.h` | the cluster DAG — also the cooked form, and this tree's Nanite |
| `world/ground/BuildingField.h` · `TileMeshes.h` · `tiles/TerrainGrid.h` | three more |
| `generators/draw/TreeMesh.h` · `TreePrototype.h` · `generators/Meshed.h` | three more |

Unreal holds `FMeshDescription` (authored) against `FStaticMeshLODResources` +
`FNaniteResources` (cooked), and `FMeshBatch` is a DRAW's description rather than a third form.
RAGE ships `grmGeometry` as the cooked form and maps its file rather than parsing it. So the
target is two forms and one cooker, and seven of the nine here are the cooked form re-invented
per producer.

`ClusterDag` is NOT one of the duplicates to remove — it is the cooked form's cluster hierarchy
and the frame path depends on it.

- [x] The value is public and names no format: `include/Geometry.h`. It is a BUILDER, not a struct
      of spans -- publishing `span<const float> PositionsM` froze the vertex layout into the ABI
      and handed out views into the producer's own vectors, valid only while it lived. The storage
      moved to `src/base/GeometryHeld.h` and stays free.
      proof: outshine/door/ScoreWhatAClientHandsIn
- [ ] The OWNING form is public too -- a caller can take one back, not only hand one in.
- [ ] Its name says what it is, not how it is stored. `Gltf::Subject` becomes the format's READER
      of that value rather than the value itself, and the rename lands in one commit with the
      reason.
- [x] A client hands outshine geometry with no file: `Engine::Stands(const Geometry &)`. The same
      nine floats reach the same frame through the glTF reader and through the door, 0 of 9216
      pixels apart.
      proof: outshine/door/ScoreWhatAClientHandsIn
**ONE PASS MOVED INTO THE PACKER, WHICH IS THE FIRST STEP OF THE MERGE AND STANDS ON ITS OWN.**
`FlatNormalsFor` -- the pass that answers a glTF primitive omitting NORMAL by splitting shared
vertices so each triangle carries its own facing -- ran only on the reader's path. The packer a
handed value goes through did not call it, so a client's or a generator's geometry without normals
was REFUSED outright: *the studio declares 1 punctual lights and an environment, and part 0 of node
'face'...*. `Assemble` runs it now, and a part stating no normals renders identically to one that
states them.
      proof: outshine/door/ScoreWhatAHandedSurfaceShows

**AND THE SECOND SPLITTING PASS FOLLOWED IT.** `BuildTangentsFor` did two jobs: read a supplied
TANGENT accessor (reader work) and GENERATE a basis when a material's normal texture needs one --
and the second splits vertices too. Its only tie to the document was one yes-or-no question, *does
this material carry a normal texture*, which is now a flag both producers fill. The generation is
`GeneratedTangentsFor(part)`, document-free, and the packer runs it.

So BOTH passes that grow the layout while walking it are the packer's now, which is exactly the
obstacle measured below. 444 Khronos cases pass with the move.

That is the shape the merge takes everywhere: a pass the reader owned becomes the PACKER's, and
both producers get it.

**AND THE VALUE IS PROVEN COMPLETE WITHOUT REWRITING THE READER FIRST.** `Subject::Handed()`
expresses what the reader produced AS a `Geometry`; assembling that gives a second subject, and the
two are identical -- 2 parts, 6 vertices, 6 indices, 1 surface, positions and normals ZERO apart,
and 0.25 metalness arriving as 0.25.

    proof: outshine/content/ScoreWhatARoundTripKeeps

That answers the item's own question the cheap way round. Filling the gap table row by row was
always the wrong repair; so was rewriting 440 lines to find out whether the value could hold what
the reader makes. A round trip ASKS: read, express, rebuild, and anything the middle form cannot
carry shows up as a difference rather than as an argument about which fields ought to be there.

The fixture carries POSITION, NORMAL, TEXCOORD_0, COLOR_0 and TANGENT across two nodes, and every
one survives: all ZERO apart. Supplied tangents were expected to fail and did not -- the value
carries a tangent stream and the reader's supplied path fills it.

**SKINS AND MORPH TARGETS DO NOT CROSS AND DO NOT NEED TO.** The reader BAKES both into the
vertices it produces, so the posed result crosses; what cannot is the ability to RE-POSE. Joints
and weights are structure, and this value carries the vertices that structure produced at one pose.
That is the right boundary for a value whose job is to be handed to a renderer -- one that could
re-pose would be a second scene graph, which is the thing this item exists to avoid having two of.

**WHAT IT COST, measured before starting and true afterwards.** `Subject::Build` was 440 lines
touching the packed arrays 26 times. Redirecting the per-primitive
EMIT into a `Geometry` part is mechanical -- positions, uv sets, colours, normals, tangents,
indices are each read and copied at `part.FirstVertex`. That half is a morning.

The half that WAS not, and is now: `FlatNormalsFor` and the generating half of `BuildTangentsFor`
ran after the emit on the packed arrays and both SPLIT vertices, growing the layout while walking
it. Both are the packer's, so the emit is all that is left of this obstacle. Morph targets and
skinning still write through the packed surface.

So this is a restructuring of the reader's PIPELINE and not a redirection of its output, and it
wants its own hour with the Khronos corpus after each sub-step: emit, then flat normals, then
tangents, then morph, then skin. 1864 vendor cases is the right oracle for exactly this and the
reason it is safe to attempt at all -- it is also why attempting it half-way is worse than not
starting: a reader that packs two ways is the second spelling this item exists to remove.

- [x] **THE glTF READER FILLS IT.** `Build` ends by expressing what it read as a `Geometry` and
      assembling THAT -- so after a read, a subject IS what the one value can carry, and anything
      the value cannot hold is dropped rather than smuggled. 444 Khronos cases pass, which is what
      says nothing is dropped.
      proof: outshine/content/ScoreWhatARoundTripKeeps

      The emit is redirected: `Build` writes its streams into a `Geometry` per primitive and
      assembles that ONCE. It no longer touches `Positions_`, `Uv_`, `Normals_`, `Tangents_`,
      `Colours_`, `Indices_` or `Parts_` at all -- the reader has stopped knowing the packed
      layout, which was the last of the two spellings. 444 Khronos cases pass.
- [x] The builder carries materials, and a handed part renders with the material it names.
      proof: outshine/door/ScoreWhatAHandedSurfaceShows
      `outshine::Material` already carried the whole PBR row and all nine `KHR_materials_*` -- it
      was on the wrong side of the door, in `src/content/shade/`. It is `include/Material.h` now,
      with `include/PunctualLight.h` beside it, and `Gltf::Subject` HOLDS the surfaces it was
      assembled with: the reader copies the document's into that list, the builder copies the
      client's, and the surface table reads the one list rather than asking a file. Textures are
      not carried yet -- an image needs the file's buffers.
      proof: outshine/door/ScoreWhatAHandedSurfaceShows
- [ ] The stored layout changes in one commit and no client recompiles (board:1954 left this
      standing: the layout moved out of the header, and that it can now MOVE is unproven).
- [x] The builder carries punctual lights and a placement per part.
      proof: outshine/door/ScoreWhatAHandedLampLights
- [ ] The builder carries hierarchy, skins, morph targets, variants and TEXTURES.
- [ ] the nine become two: every producer fills the AUTHORED form, one cooker makes the cooked
      one, and `ClusterDag` is part of the cooked form rather than a ninth mesh
- [ ] `geometry` is a tier of its own between `math` and everything that carries shape
- [ ] A generator fills the same value (board:1948) -- one value, three producers, proven by a
      case that stands the same geometry each way and compares the pictures.
- [ ] The glTF SERIALISER writes it back out, so the round trip closes: fill, serialise, read,
      and the second value equals the first. Negative control: a field the serialiser drops, and
      the comparison names it.

## THE VALUE IS NOT A CONTAINER, IT IS A GUARANTEE

One value shared by every producer is half the decision. The other half is what the value REFUSES to
represent, and it is the half that would have prevented every defect board:2031 has been closing by
hand: a producer says what the shape IS and the engine answers for what it BECOMES.

**Benchmark** — Unreal: a producer fills `FMeshDescription`; `FStaticMeshBuilder` owns the welding,
the tangent basis, the render-vertex split and the validation, and a non-manifold edge FAILS THE
BUILD. glTF, FBX, Datasmith and a procedural tool all go through that one door. RAGE: the exporter
validates and refuses, and every asset shipped had passed it. **They agree completely**, so only the
shape here was mine.

### Why, measured rather than argued

Rothenburg, from the buildings alone:

    2 781 105   corners emitted
      975 372   identical in POSITION AND NORMAL -- pure duplication, 35 per cent
    1 805 733   distinct, which is what a vertex buffer should hold
      494 944   distinct POSITIONS, which is what the topology should hold
       58 395   edges on one triangle, so holes
       20 630   edges on more than two, so not a surface

Every producer in this tree emits a flat triangle SOUP. Nothing relates one corner to another, so
closedness is not a property anything HAS — it is reconstructed afterwards by a walk that lives in a
test. Five builders wrote their own polyline along one footprint edge and nothing could notice they
disagreed until that walk welded them and counted. **With one description shared by all of them,
none of the defects this session closed could have existed.**

### The shape of it

A `MeshBuilder` in `src/base/`, below the generator tier, that a producer cannot go around because
`StructureMesher::Mesh` takes one instead of a `std::vector<float>&`.

    what a producer may say          what it may NOT do
    ----------------------------     -------------------------------------------
    at(E, N, Z) -> Position          push a float
    loop(span<Position>, outward)    choose its own winding
    quad(a, b, c, d)                 emit triangles at all
    attributes per face-corner       delete, drop, or refuse a face of its own

Six things the engine then owns, once:

1. **SNAPPING, not welding.** `at()` quantises to a millimetre and returns a handle. Two corners
   meant to be one corner ARE one handle, by construction. Welding repairs a drift that snapping
   never allows to start.
2. **The topology.** Positions and faces, half-edges derived. A shared edge is one object.
3. **Triangulation.** A producer states a LOOP; the engine cuts it. Two producers meeting on one
   boundary state the same loop and cannot disagree about how it is subdivided.
4. **Winding.** A producer states which side is outward; the engine orders the corners. A face
   cannot be inside out.
5. **The render-vertex split.** Positions welded for topology, render vertices split where a normal
   or a UV differs. That is the 35 per cent above, recovered once and for everybody.
6. **Reduction, never deletion.** Where a face is too small to carry a pixel the engine collapses an
   edge under an error bound and the shell stays closed. Nothing is ever discarded — dropping a
   triangle takes three edges out of the walk and can hide the hole beside it, which is measured in
   board:2031 and cost this tree a false "whole" reading.

**And the engine REFUSES.** `Closed()` is answered by the builder about itself, at build time, with
the producer named. A hole stops being a number a test finds later and becomes a refusal on the day.

### The measurements that would show this half is wrong

1. **The corner count.** 2 781 105 emitted today against 1 805 733 distinct. If the split is done
   once by the engine the vertex buffer holds the smaller number and the index buffer the larger; if
   the emitted count does not fall by about a third, the split is not happening
2. **The negative control is a producer that tries to lie.** A test producer that states an open
   loop, a face with two corners in one place, or two faces on one edge with the same winding — the
   build must REFUSE each, by name. If any of them gets through, the guarantee is a wish
3. **Cost, as a BOUND and not a tick**: `apps/bench`. A hash lookup per corner is not free and the
   frame budget is what it is. Quoted in whatever item spends it
4. **Every producer, not one.** glTF, terrain, OSM and vegetation all through it. If any keeps a
   private path to a float buffer, the guarantee has a hole in exactly the shape of that path
