Type: issue
State: open
Area: render, clients
Tags: layering, door, measured
Supersedes: 1535, 1543

# The renderer has a door, and `src/engine/` is not it

`src/engine/GltfStudio.{h,cpp}` is the ONLY bridge from geometry to `Renderer`, and it holds
every convention the renderer requires while publishing none of them: the world is ECEF and a
mesh carries an anchor near its own geometry (`kStudioAnchorEcefM`, GltfStudio.h:20); a camera
needs basis AND fov AND near before it sees anything; vertices arrive interleaved at
layout-dependent offsets; materials, lights and environment go in before placements. Six attempts
to hand the renderer geometry were made and only two were declarations it could have refused.

Two shapes underneath say the same thing. `struct Studio {` (GltfStudio.h:26) carries
`const Gltf::Subject *Geometry = nullptr;` (:27) — ONE pointer, so the picture holds exactly one
thing and a road and a car cannot stand in it together. And `struct StudioScratch {` (:49)
beside it is the studio and its scratch as two spellings of one stand-up.

Generated geometry has no entrance at all: whoever has a file builds a subject from it, whoever
has a generator builds a subject from that, and neither may serialise a GLB to get in.

## What will be true

- [ ] ONE door hands geometry to the renderer without spelling glTF, and it takes 1..N placed
      subjects with the `Model[16]` a draw already carries — a subject enters and leaves without
      the others being re-stood (board:1574).
- [ ] Every convention above is a DECLARATION the renderer refuses by name when it is
      contradicted, not a convention a bridge remembers.
- [ ] `src/engine/` is dissolved: the public door stays, the scaffolding goes, and the three
      headers nothing includes are deleted.
- [ ] Proving test: a case hands the renderer a triangle through the door and reads back a
      pixel; every named refusal `SetMesh` returns has a case.
