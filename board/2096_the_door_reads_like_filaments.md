Type: chore
State: open
Area: include
Tags: measured

# A reader who knows Filament finds every door name where Filament puts it

**Benchmark** — Filament ships one header per public type: `Engine.h`, `View.h`, `Camera.h`,
`Scene.h`, `Material.h`, and groups them under `filament/`, `math/`, `utils/`. Cesium does the same
with `CesiumGltf/Model.h`. Unreal has no namespaces and answers the same question with a prefix
system (`U`, `A`, `F`) plus one class per file. **All three agree that a public type is findable by
its own name**, and this tree does not yet.

## Measured 2026-09-01, after board:2093 grouped include/ and named the schema

| what | now | what a reference would do |
|---|---|---|
| `include/scenario/Scenario.h` | 638 lines, **54 types** | Filament: `View.h`, `Camera.h`, one per type |
| the header's name | `Scenario.h` | its root type is `Scenario::Document`, so `Document.h` |
| `include/scene/Geometry.h` | holds `Geometry`, and three managers | Filament splits `RenderableManager.h`, `TransformManager.h`, `LightManager.h` |
| `Geometry` the type | parts, surfaces, lamps | in Filament that word means VERTEX DATA; ours is Cesium's `Model` |
| `Loaded` the type | reads a file, holds animations and cameras | Filament calls this `FilamentAsset` |

The last two are RENAMES that board:2093 could not make, and the reason is written down rather than
guessed: `Asset` and `Camera` were taken by the schema until it got its namespace, and they are free
now. That is the whole reason this item exists as a separate one.

## The cut, and why these lines

Splitting 54 types by file is not the goal -- a header per type would put `Falls` and `Makes` in
files of their own. The cut is by **what a client is doing when it reaches for the name**:

| header | holds |
|---|---|
| `scenario/Document.h` | the root, `Identity`, `Layer`, `Persisted`, `Binding`, `Clock` |
| `scenario/View.h` | `View`, `Camera`, `Patch` |
| `scenario/World.h` | `Georeference`, `Weather`, `Relief`, `Structure`, `WorldSettings`, `Provider`, `Setting`, `Generator`, `Compositor` |
| `scenario/Render.h` | `RenderPlan`, `Lighting`, `Light`, `SurfaceOverride` |
| `scenario/Body.h` | `Body`, `Drive`, `Slip`, `Contact`, `Prismatic`, `Slot`, `Journey`, `Player`, `PhysicsSettings`, `Standing`, `Placement` |
| `scenario/Mind.h` | `Mind`, `Kind`, `Instance`, `Region`, `Door`, `Volume` |
| `scenario/Sound.h` | `Emitter`, `Voice`, `Sound`, `Room`, `Bus`, `Falls`, `Makes` |
| `scenario/Asset.h` | `Asset`, `AssetAnimation`, `Surface`, `Table`, `Event` |

## What measurement shows this was wrong

`make doc` counts undocumented public entities and 359 of the 719 are in Scenario.h alone. A header
that carries half the door's undocumented surface is a header nobody has read end to end, which is
the same finding from the other side.

## Done when

Every public type is in a header named for what a client would look under, `Geometry` is `Model`,
`Loaded` is `Asset`, and a stranger asked to find the camera declaration finds it without grep.
