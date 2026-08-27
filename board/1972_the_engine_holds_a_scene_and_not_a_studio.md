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

| what `Studio` holds | where it belongs | why |
|---|---|---|
| part placements, emitted radiance, part-surface indices, punctual lights | `src/scene` columns by handle | plain values over `base` types; `Column.h` ALREADY EXISTS and nothing reaches it |
| `Render::SubjectMaterial`, `Render::SubjectEnvironment`, the previous pose | the renderer's own proxy | Unreal's split exactly: the scene holds a surface HANDLE, the renderer holds the material |
| `Gltf::Subject *` | content, reached by handle | the scene names an asset; it does not point into one |
| eye, `EyeStandsInside`, `FramedParts` | the camera | framing is optics and not scene state |
| `EcefFromGltf`, `PlacedInEcef` | `src/content/gltf` | a format's own axis convention |

`src/scene/Column.h` and `src/scene/Store.h` are a capability that is COMPLETE and unreachable --
the dominant defect class in this tree, and here it is the reason a 559-line god struct was
written beside it.

**RAGE and Unreal both settled this and neither settled it this way.** Unreal keeps `FScene` beside
`UWorld` and the renderer's copy is fed by primitive-level deltas from `FPrimitiveSceneProxy`;
RAGE keeps the draw-side scene apart from the entity and updates it per entity. In both, the
renderer's scene is a THING WITH A DOOR that the world pushes into. In neither is it a struct of
public vectors named after the file format that happened to fill it.

- [ ] `Studio` dissolves: placements, surfaces, lights and environment become the scene's columns
      behind a door; eye and framing go to the camera; the glTF axis convention goes to `content/`
- [ ] the placement diff is spelled ONCE and then not at all -- the writer states what it moved
- [ ] `Move` stops touching placements: a pose update is vertices, and a placement is not a vertex
- [ ] no type in `src/engine` is named after a content format
- [ ] proof: the drive's trajectory is unchanged and the three-producer frame is bit-identical
