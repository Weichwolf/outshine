Type: feature
State: open
Parent: 1953
Progress: door
Area: door, content, generators
Tags: benchmark, target, owner

# ONE value carries 3D data into and out of outshine, and glTF is one of its file forms

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
| positions, normals, uv0, uv1, tangents, colours, indices | yes |
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

## What will be true

- [x] The value is public and names no format: `include/Geometry.h`. It is a BUILDER, not a struct
      of spans -- publishing `span<const float> PositionsM` froze the vertex layout into the ABI
      and handed out views into the producer's own vectors, valid only while it lived. The storage
      moved to `src/base/GeometryHeld.h` and stays free.
      proof: harness/outshine/door/ScoreWhatAClientHandsIn
- [ ] The OWNING form is public too -- a caller can take one back, not only hand one in.
- [ ] Its name says what it is, not how it is stored. `Gltf::Subject` becomes the format's READER
      of that value rather than the value itself, and the rename lands in one commit with the
      reason.
- [x] A client hands outshine geometry with no file: `Engine::Stands(const Geometry &)`. The same
      nine floats reach the same frame through the glTF reader and through the door, 0 of 9216
      pixels apart.
      proof: harness/outshine/door/ScoreWhatAClientHandsIn
- [ ] **THE glTF READER FILLS IT.** `Gltf::Subject` stops being a second representation and
      becomes the reader OF this one, so what the reader takes and what the builder carries are
      the same list by construction rather than by maintenance.
- [ ] The builder carries materials, and a handed part renders with the material it names.
- [ ] The stored layout changes in one commit and no client recompiles (board:1954 left this
      standing: the layout moved out of the header, and that it can now MOVE is unproven).
- [ ] The builder carries punctual lights, hierarchy, skins, morph targets and variants.
- [ ] A generator fills the same value (board:1948) -- one value, three producers, proven by a
      case that stands the same geometry each way and compares the pictures.
- [ ] The glTF SERIALISER writes it back out, so the round trip closes: fill, serialise, read,
      and the second value equals the first. Negative control: a field the serialiser drops, and
      the comparison names it.
