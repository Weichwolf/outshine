Type: feature
State: open
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

## What will be true

- [ ] The value is public: a header under `include/` declares the view a caller fills and the
      owning form outshine keeps, naming no format.
- [ ] Its name says what it is, not how it is stored. `Gltf::Subject` becomes the format's READER
      of that value rather than the value itself, and the rename lands in one commit with the
      reason.
- [ ] A client hands outshine geometry with no file: it fills the view, the engine takes it, and
      the picture draws it. Proving case: a scenario standing a subject a client built in memory,
      against the same oracle a read-from-file subject passes.
- [ ] A generator fills the same value (board:1948), and so does the glTF reader -- one value
      with two producers, proven by a case that stands the same geometry both ways and compares
      the two.
- [ ] The glTF SERIALISER writes it back out, so the round trip closes: fill, serialise, read,
      and the second value equals the first. Negative control: a field the serialiser drops, and
      the comparison names it.
