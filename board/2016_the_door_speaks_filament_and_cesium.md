Type: feature
State: active
Progress: door
Area: door
Tags: benchmark, target

# The door speaks FILAMENT and CESIUM, and a reader already owns its vocabulary

**Benchmark** — **Filament** (Google, Apache 2.0) is a RENDERER rather than an engine, which is the
exact layer this door exposes, and the people who wrote it ship it on phones — this target's own
constraint. Its vocabulary: `Engine` · `Scene` · `View` · `Camera` · `Renderer` · `Material` ·
`MaterialInstance` · `TransformManager` · `Skybox` · `IndirectLight`. **Cesium** (Apache 2.0) is the
reference for georeferenced 3D and the one Unreal and Unity users actually spell a place with:
`Georeference` · `GlobeAnchor` · `LongitudeLatitudeHeight` · `sampleHeight`. **Taking both, and
CLAUDE.md now carries the rule**: outward the names a client expects, inward the engineering RAGE
and Unreal paid for.

**Not Godot**, and the reason is a rule rather than a preference: its vocabulary is a NODE TREE and
a scenario over a store has no such hierarchy. A name is a promise.

## What the door says today, and what it would say

Measured from `include/Outshine.h` and `include/Scenario.h`:

| today | Filament / Cesium | why it matters |
|---|---|---|
| `Engine::DrawsInto(window)` / `(extent)` | `Renderer` + `SwapChain` | two verbs with one name, and neither says which is the swap chain |
| `Engine::RenderTo(extent)` | `Renderer::render(View)` | a frame is rendered THROUGH a view, and here the view is implicit |
| `Engine::Stands(Geometry)` | `Scene::addEntity` | "stands" is this tree's word and nobody else's |
| `Engine::Shows(surfaces)` | `View::setViewport` / overlay | one verb doing two things |
| `Scenario::View` | `Camera` **and** `View` | Filament splits them: a `Camera` has a projection, a `View` has a scene, a camera and a viewport. This door fuses them and that is why a camera could not be placed without a body |
| `Standing.AtM[3]` | `TransformManager` + `GlobeAnchor` | three metres in a frame **nobody names** -- measured: it cost an hour of guessing to place one eye |
| `Standing.GlobeAnchor/LongitudeDeg/LatitudeDeg/HeightM` | already Cesium's | landed while finding the above |
| `Light` | `LightManager` + `IndirectLight` | a key light and a sky term are different things and share one struct |

## What will be true

- [ ] a `Camera` is a thing of its own with a projection, and a `View` binds a scene, a camera and
      a viewport -- Filament's split, which is what makes a camera placeable without a body
- [ ] a placement is a `GlobeAnchor` with `LongitudeLatitudeHeight` wherever anything is placed --
      body, view, instance, volume -- rather than three metres in an unnamed frame
- [ ] `sampleHeight` puts a thing on the TERRAIN, because a client asks for ground level and does
      not compute it
- [ ] the door's verbs are Filament's where Filament has one, and this tree's only where it does
      not -- each exception named in this item with its reason
- [ ] `apps/demo`'s line count FALLS, because a client that already knows Filament writes less

**The measurement that would show I am wrong:** `apps/demo` is 194 lines and `apps/bench` 339. If
the rename does not reduce them, the door was not the thing making a client verbose and the item's
premise was wrong. `test/run.sh` counts both today, so the before-number exists.
