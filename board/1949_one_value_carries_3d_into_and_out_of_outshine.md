Type: feature
State: open
Parent: 1953
Progress: door
Area: door, content, generators
Tags: benchmark, target, owner

# ONE value carries 3D data into and out of outshine, and glTF is one of its file forms

**Benchmark** — Unreal: `FMeshDescription` authored against `FStaticMeshLODResources` + `FNaniteResources` cooked. RAGE: `grmGeometry` cooked, and its file IS that form. **Both agree** — two forms and one cooker; this tree has nine.

Owner's target, arrived at over three exchanges and stated here whole:

- generators do not serialise; they hand back the internal representation (board:1948)
- a glTF serialiser ships beside them, for a caller who wants a file
- **that representation is the UNIVERSAL interface for exchanging 3D data with outshine**

The third sentence is the one that changes the shape. The value is not "what a generator returns"
-- it is what ANYTHING hands outshine and what outshine hands back. A glTF reader fills it. A
generator fills it. A foreign program fills it directly, with no file anywhere. The compositor
consumes it. The serialiser writes it out. One value, many producers, many consumers.

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
