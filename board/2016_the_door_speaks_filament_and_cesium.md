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

## Our own verbs do not align by themselves, and three of four decisions go to Filament

Filament is a RENDERER: it has no scenario, no simulation, no mixer, no store. `Declare` · `Read`
· `Assemble` · `Advance` · `Run` · `Mixes` · `Park` · `Resume` · `Numbers` · `Inspects` are this
tree's own by right and are not a debt -- `STATE.md` says so beside the distance table, because
without that line the page would read forty per cent for ever and mean nothing.

But "ours" settles the WORD, not the SHAPE, and the shape does not converge on its own:

| | Filament | here | taken |
|---|---|---|---|
| types | `PascalCase` | `PascalCase` | already the same |
| methods | `camelCase` -- `beginFrame`, `setScene` | `PascalCase` -- `Declare`, `Advance` | **camelCase AT THE DOOR.** Inside stays PascalCase; the translation happens once at the boundary, which is the rule this item is built on |
| verb form | imperative, saying what it does to what -- `addEntity(Entity)` | elliptic present -- `Stands(Geometry)`, `Shows(surfaces)` | **the ellipsis goes at the door.** It is the single thing that makes a reader guess, measured: placing one eye cost reading three files because `Stands` and `Placed` say nothing about what they take |
| refusal | pointers and void, errors reported elsewhere | `[[nodiscard]] bool` + `Error()` | **stays ours**, and with a reason: CLAUDE.md's own rule -- `std::expected` where a refusal carries its reason -- is stronger than Filament's convention, and a door that cannot say WHY is the defect this session fixed four times |

- [ ] the door's methods are `camelCase` and its types `PascalCase`, Filament's split exactly
- [ ] no door verb is elliptic: each says what it does and to what
- [ ] a refusal carries its reason, which is `std::expected` rather than Filament's silence

**The measurement**: `STATE.md` counts the spoken names and verbs at every `make`. If the count
stops moving while this item is active, the rename has stalled and the page says so without anyone
remembering to check.

## Stage two's first reading, measured rather than guessed

The five places stand, fetch and draw. What they draw is not their place, and the numbers say where
the fault is NOT:

    GrandCanyon  9 tiles, 18 166 triangles, 3475 m relief over 3659 m, EYE up 2185.8 m
    Shibuya      9 tiles, 18 642 triangles, 3401 m relief over 3243 m
    Venice       9 tiles, 13 148 triangles, 2911 m relief over 2911 m
    BlackForest  9 tiles, 16 356 triangles, 2720 m relief over 2282 m
    OldTown      9 tiles, 13 404 triangles, 2675 m relief

**The camera is right.** Mather Point stands at about 2 100 m and the eye reads 2 185.8 m -- that is
`sampleHeight` asking the terrain and adding the sixty metres the case declared. Moving it to
3 000 m and pitching it to -85 degrees changes the picture by three thousand pixels of nine hundred
thousand, which is the next thread rather than a camera fault.

**The heights arrive and they are WRONG.** A 3 km patch of Shibuya carries 3 401 m of relief and
Tokyo is flat; Venice reads 2 911 m at sea level. Either the DEM decode is off, or void samples are
mapped to an extreme and a handful of spikes own the range. `min` and `max` over the ring cannot
tell those apart, and the next measurement is a HISTOGRAM rather than a span.

**And the picture is a plane** while the ring says 3 475 m of relief across 3 659 m. Those two
cannot both be true of the same vertices, so the geometry the subject stage draws is not the
geometry the compositor measured -- which is the reading stage two starts from.

Written down before the work, so being wrong is visible: I expect the DEM decode to be the fault
and the spikes to be its symptom. If the histogram comes back smooth and the relief is real, the
fault is between the compositor and the stage instead, and this paragraph is what I got wrong.
