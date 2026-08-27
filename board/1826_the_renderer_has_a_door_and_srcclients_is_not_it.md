Type: issue
State: open
Area: render, clients
Tags: layering, door, measured
Supersedes: 1535, 1543

# The renderer has a door, and `src/engine/` is not it

**Benchmark** — Unreal: `FScene` and `FSceneRenderer` behind a door the world pushes into. RAGE: the draw list is separate from the device. **Taking Unreal** — the renderer's conventions must be refused at the door rather than learned by getting a call order right.

**Half of this closed with board:1972 and the other half stands.** The bridge is
`src/render/SubjectProxy.{h,cpp}` now, in the tier that may hold it; the anchor is a value the
engine passes rather than a planet radius wired into a rendering helper; the view left the proxy;
the proxy has a door and its per-part tables are sized from the subject it stands over; and the
studio-and-its-scratch double spelling is one value plus one scratch with a stated purpose.

**What did NOT change: the proxy still holds ONE `const Gltf::Subject *`.** The picture holds
exactly one thing, so a road and a car cannot stand in it together, and every convention the
renderer requires is still learned by getting the order right rather than by being refused --
materials, lights and environment before placements; basis and fov and near before anything is
seen; vertices interleaved at layout-dependent offsets.

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
