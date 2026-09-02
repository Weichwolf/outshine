Type: chore
State: open
Area: include
Tags: measured

# A reader who knows Filament finds every door name where Filament puts it

**Benchmark** — Unreal answers this with a PREFIX system and one class per file (`UWorld`,
`AActor`, `FVector`); RAGE answers it with a two-letter subsystem prefix on every type (`grcTexture`,
`fwEntity`, `rage::sysTaskManager`), so a name says which library owns it before you open anything.
The two agree on the property and differ on the mechanism, and this tree takes the third that both
of its door's sources take -- a NAMESPACE plus one header per type. Filament ships one header per
public type: `Engine.h`, `View.h`, `Camera.h`,
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

## The SHAPES, measured 2026-09-02 -- and they are a separate half of the same debt

Where the names go is above. This is what a client PASSES, and clang-tidy found it before I did:
five door verbs take a run of bare doubles that a caller can transpose in silence.

| the verb | now | what the reference does |
|---|---|---|
| `Camera::setProjection` | `(fovDeg, nearM, farM)` and `(left, right, bottom, top, near, far)` | Filament leads the second with `Projection::ORTHO`, so the two are told apart by a NAME rather than by counting arguments |
| `Camera::setExposure` | `(apertureFStops, shutterS, sensitivityIso)` | Filament's is the same three, and Filament would be flagged here too |
| `Engine::sampleHeight` | `(latitudeDeg, longitudeDeg, heightM&)` | Cesium passes a `Cartographic`; this tree already HAS `LongitudeLatitudeHeight` and does not use it at its own door |

**The ortho overload is worse than a swap risk: it DROPS what it is handed.** It takes the four
frustum edges and keeps `0.5 * (right - left)` and `0.5 * (top - bottom)`, so a client that declares
an off-centre frustum gets a centred one and is told nothing. The one caller in this tree
(`Render::CameraOf`) passes `-XMagM, +XMagM, -YMagM, +YMagM` -- it builds the symmetry the setter
immediately undoes. Accepting a declaration and doing something else with it is the failure
CLAUDE.md names by name.

**The answer is glTF's camera, because that is where the numbers come from**: `perspective` is
`{yfov, znear, zfar}` and `orthographic` is `{xmag, ymag, znear, zfar}` -- half-extents, never
edges. Two named records, two overloads told apart by TYPE, and the asymmetry that cannot be held
is refused at the door instead of silently centred.

## `Renderer::render(Extent)` is the one STRUCTURAL question here, and it is not a rename

Filament's is `Renderer::render(View*)` -- the view carries its own viewport, and rendering names
what it draws. Ours is `Engine::setView(std::string_view)` followed by `Renderer::render(Extent)`:
WHICH view is engine state and HOW BIG is the argument, which is the split Filament does not make.
Whoever takes this states which of the two is right for a tree whose views are DECLARED by id in a
scenario -- a `View*` a client holds is a handle into a document it does not own, and that may be
the reason ours differs rather than an oversight. An item that cannot say which is not understood
yet.


## A word that means two things: 14 types and 2 constants, measured 2026-09-02

`harness/claims/EveryTypeNameIsDeclaredOnce` declares a ceiling of 9 and 1; the tree stands at 14
and 2, so this half is a REGRESSION and the ceiling was raised by work that did not look.

| the word | where it is declared |
|---|---|
| `Node` | `ui/Markup.h` · `import/Types.h` · `generators/draw/TreeSkeleton.h` · `base/format/Json.h` · `base/format/Script.h` · `base/format/Xml.h` · `base/spatial/Capacity.h` · `base/spatial/Wayfinding.h` |
| `Document` | `import/Variant.h` · `import/Document.h` · `import/Pose.h` · `import/Subject.h` · `scenario/Scenario.h` |
| `Attribute` | `ui/Markup.h` · `import/Types.h` · `import/Emit.cpp` · `base/format/Xml.h` |
| `Value` | `ui/Style.h` · `world/ground/OsmField.h` · `base/format/Script.h` |
| `Request` | `world/data/Request.h` · `generators/draw/TreeGrower.h` · `generate/Generate.h` |
| `Scene` | `scene/Scene.cpp` · `import/Types.h` · `scene/Scene.h` |
| `Body` | `generators/base/ContactMaterial.h` · `generators/base/BodyId.h` · `scenario/Scenario.h` |
| `Attitude` | `actor/mind/Fly.h` · `Earth.h` |
| `Camera` | `import/Viewport.h` · `scenario/Scenario.h` |
| `Declaration` | `ui/Style.h` · `engine/Live.h` |
| `Generator` | `generate/Generate.h` · `scenario/Scenario.h` |
| `Host` | `base/format/Script.h` · `scenario/Event.h` |
| `Instance` | `generators/draw/ClusterId.h` · `scenario/Scenario.h` |
| `Sampler` | `import/Types.h` · `scene/Texture.h` |

**Five of them were added by the door's own renames** -- `Document`, `Request`, `Attitude`,
`Camera`, `Generator` -- and that is the finding: a rename that makes ONE header read better and
does not look at the tree buys its clarity with somebody else's. The repair is not a regex; it is
per-word, and each one has to say which meaning keeps the word and what the other becomes. `Node` is
the loudest at eight files, and a `Node` in a JSON reader and a `Node` in a road graph share nothing
but four letters.
