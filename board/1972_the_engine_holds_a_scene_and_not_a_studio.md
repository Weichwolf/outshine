Type: issue
State: active
Parent: 1953
Area: architecture

# The engine holds a SCENE, not a glTF studio

`src/engine/GltfStudio.{h,cpp}` -- 559 lines -- is a content format and a photography metaphor
standing where the engine's scene belongs. It is not a detail of board:1957; it is that item's
GROUND, because a god struct cannot be given a clean delta path.

What it holds, measured:

| finding | measurement |
|---|---|
| the name | `Gltf` is a content FORMAT and `Studio` is a turntable. The engine knows bodies, placements, cameras and lights, and neither of those two words is one of them |
| a god struct | `Studio` carries ELEVEN public data members -- geometry pointer, eye, framing, emitted radiance, part surfaces, part placements, surfaces, previous pose, lights, environment. That is the whole scene in one struct with no door, and CLAUDE.md's sentence applies exactly: a public data member is an invariant nobody can hold |
| the layer | `EcefFromGltf` and `PlacedInEcef` are the axis convention BETWEEN a content format and world space, and they stand in `engine/`, the top tier. A format's own convention belongs to the format |
| it is already the scene store | TARGET declares `Scene Store -- entities / pairs / traits / tags / slots` and `Columns -- placements, by handle`. `Studio` IS that, under the wrong name and without encapsulation, which is why it grew eleven members instead of a door |
| it carries a doubled truth | the placement diff is spelled TWICE, at `Live.cpp:634` and `GltfStudio.cpp:39`, byte for byte the same loop over sixteen doubles per row |
| the frame path pays for it | `Live::Submit` calls `Move` EVERY frame, which rebuilds the entire placement table and diffs it -- to discover what the one writer, `Live::Carry`, already knew |

**ITS ADDRESS IS THE SYMPTOM.** `src/engine` is the ONLY tier that reaches everything, and that is
exactly why `Studio` is there: it holds a `Gltf::Subject *` (content), a `Render::SubjectMaterial`
and a `Render::SubjectEnvironment` (render), and placements and radiance (base). No lower tier may
hold all four, so the struct floated up until it found the one address where its incoherence is
legal. `src/scene` reaches `base` and nothing else -- so the tier table already states the
decomposition, and it needs no invention:

**AND THE FIRST DECOMPOSITION IN THIS ITEM WAS WRONG.** It said the per-part tables become
`src/scene` columns. They do not: placements, emitted radiance, surface slots and lights are what a
RENDERER must know to draw one subject, not what the world knows about it -- `Live::Carry` proves
it by writing every part from exactly two world matrices. The world's truth is the body transform;
the per-part table is the renderer's expansion of it. That is Unreal's split (`UWorld` holds the
transform, `FPrimitiveSceneProxy` holds what draws), and the tier table permits it exactly:
`src/render` reaches `base` and `content`, which is all the type needs once the hardcoded planet
radius becomes a value the engine passes in.

`src/scene/Column.h` and `Store.h` remain a COMPLETE capability nothing reaches. That is a real
finding and it is not this one: entities and their relations are what belongs there, and no
declaration reaches them yet.

**RAGE and Unreal both settled this and neither settled it this way.** Unreal keeps `FScene` beside
`UWorld` and the renderer's copy is fed by primitive-level deltas from `FPrimitiveSceneProxy`;
RAGE keeps the draw-side scene apart from the entity and updates it per entity. In both, the
renderer's scene is a THING WITH A DOOR that the world pushes into. In neither is it a struct of
public vectors named after the file format that happened to fill it.

- [x] the god struct leaves the top tier: `src/engine/GltfStudio.{h,cpp}` is
      `src/render/SubjectProxy.{h,cpp}`, and the planet radius that pinned it there is a value the
      engine passes in. Proof: `--audit-layers` green with the file under `src/render`
- [x] it has a DOOR: thirteen public data members are private behind `Stands`, `Sees`, `Wears`,
      `Emits`, `Places`, `Lit`, `Around`, and two of the five submission-time checks are gone
      because what they forbade cannot be constructed
- [x] the proxy stops holding the VIEW: `Render::View` carries eye, whether the camera stands
      inside the subject, and how many parts frame it; `Show`, `Surface`, `Place`, `Move` and
      `Aim` take it beside the proxy. Unreal keeps this in `FViewInfo` and never in a primitive's
      proxy, and the tree had it spelled TWICE in `Live` -- the declared eye and the resolved one
- [x] the placement diff is spelled ONCE and then not at all -- the writer states what it moved
- [x] `Move` stops touching placements: a pose update is vertices, and a placement is not a vertex
- [x] no type in `src/engine` is named after a content format, and no namespace there names a
      CLIENT: `outshine::Clients` is `outshine::Core`
- [ ] proof: the drive's trajectory is unchanged and the three-producer frame is bit-identical
