Type: chore
State: open
Area: include
Tags: measured, door

# A reader who knows Filament finds every door name where Filament puts it

**Benchmark** -- Unreal answers this with a prefix system and one class per file (`UWorld`,
`AActor`); RAGE with a two-letter subsystem prefix on every type (`grcTexture`, `fwEntity`).
The two agree on the property and differ on the mechanism, and this tree takes the third that
both of its door's sources take: a NAMESPACE plus one header per public type, grouped as
Filament groups `filament/`, `math/`, `utils/`. **All three agree that a public type is findable
by its own name**, and this door is not yet.

## Where it stands, measured 2026-09-04

| what | now | reference |
|---|---|---|
| `include/scenario/Scenario.h` | 708 lines, 51 top-level types | Filament: one header per type a client reaches for |
| `Geometry` | parts, surfaces, lamps | Filament's word for VERTEX DATA; ours is Cesium's `Model` |
| `Loaded` | reads a file, holds animations and cameras | `FilamentAsset` -> `Asset` |
| `Engine::setView(id)` + `Renderer::render(Extent)` | which view is engine state, how big is the argument | Filament: `Renderer::render(View*)` |
| `SwapChain::logsTo` | DECLARED at `Outshine.h:68`, defined nowhere | a dead door declaration |
| `Engine::logsTo` | `static`; the sink is process-wide | a free `outshine::logsTo` beside `LogSink` |
| a word declared twice | 9 type names, exactly at the claim's ceiling: `Node` `Document` `Attribute` `Value` `Scene` `Host` `Declaration` `Camera` `Sampler` | one meaning per word in `include/` |

Done and holding: `Camera::Perspective` / `Ortho` / `Exposure` as named records chosen by type
(glTF's shape, half-extents never edges); `sampleHeight(const LongitudeLatitudeHeight &)`.

## The cut, by what a client is doing when it reaches for the name

| header | holds |
|---|---|
| `scenario/Document.h` | the root, `Identity`, `Layer`, `Persisted`, `Binding`, `Clock` |
| `scenario/View.h` | `View`, `Camera`, `Patch` |
| `scenario/World.h` | `Georeference`, `Weather`, `Relief`, `Structure`, `WorldSettings`, `Provider`, `Setting`, `Generating`, `Compositor` |
| `scenario/Render.h` | `RenderPlan`, `Lighting`, `Light`, `SurfaceOverride` |
| `scenario/Body.h` | `Body`, `Drive`, `Prismatic`, `Slot`, `Standing`, `Placement`, `PhysicsSettings` -- after board:2127 has taken the tyre out |
| `scenario/Mind.h` | `Mind`, `Kind`, `Instance`, `Region`, `Door`, `Volume` |
| `scenario/Sound.h` | `Emitter`, `Voice`, `Sound`, `Room`, `Bus`, `Falls`, `Makes` |
| `scenario/Asset.h` | `Asset`, `AssetAnimation`, `Surface`, `Table`, `Event` |

`Renderer::render(Extent)` is the one STRUCTURAL question and the answer is written here so it
is not re-argued: the view stays engine state. A scenario DECLARES its views by id and the engine
owns the document; a `View*` a client holds would be a handle into a document it does not own,
and that is the reason ours differs from Filament rather than an oversight. `render(Extent)`
keeps the canvas as the argument because the canvas is the client's.

## What will be true

- [ ] Every public type stands in the header above; `Scenario.h` is an umbrella include and
      nothing else
- [ ] `Geometry` is `Model`, `Loaded` is `Asset` -- renamed at the declaration, callers named by
      the compiler
- [ ] `SwapChain::logsTo` is gone; `outshine::logsTo(LogSink *)` is the one door onto the sink
- [ ] The nine collisions are decided per word -- which meaning keeps it, what the other becomes
      -- and the claim's ceiling falls to 0
- [ ] `make doc` reports 0 undocumented entities in `include/scenario/` (board:2131 holds the
      rest)

## What will show I was wrong

A client in the tree -- `src/client/` -- that gets LONGER after the cut. The door is measured by
the client's line count and a split that costs the client includes is the wrong split.
