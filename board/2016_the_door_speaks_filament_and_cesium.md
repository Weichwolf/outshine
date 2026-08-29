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

- [x] a `Camera` is a thing of its own with a projection, and a `View` binds a scene, a camera and
      a viewport -- Filament's split. PROVEN by the five places, which put an eye on the Chasseral
      with no body under it, and whose negative control is the failure that filed this line: a
      camera could not be placed without one
- [x] a placement is a `GlobeAnchor` with `LongitudeLatitudeHeight` wherever anything is placed --
      it sits on `Standing`, which body, view, instance and placement all carry
- [x] `sampleHeight` puts a thing on the TERRAIN, and it is a VERB on the door rather than a flag on
      a declaration. `Engine::sampleHeight(lat, lon, out)` reads 2 125.8 m at Mather Point and
      REFUSES before a world stands, which is its own negative control: a door answering 0.0 there
      would satisfy the positive check by accident. Proven by
      `test/outshine/door/ScoreWhatTheGroundAnswers`
- [x] the door's verbs are Filament's where Filament has one, and this tree's only where it does
      not -- each exception named below with its reason
- [~] ~~`apps/demo`'s line count FALLS, because a client that already knows Filament writes less~~
      **WITHDRAWN, and the withdrawal is the finding.** Naming a thing that had no name COSTS a line:
      `Renderer renderer = engine.renderer();` is Filament's own shape and every client that draws
      pays it once -- demo 194->195, bench 359->360, driver 253->254, viewer 338->339. The earlier
      reading this item carried (demo 194->170, bench 339->343) does not reproduce; it stood over
      three different code states, which is the same defect board:2026 was withdrawn for. A door is
      judged by whether a client has to GUESS, and a line count cannot see that -- it is a rate, and
      a rate has no negative control. `outshine/door/ScoreWhichWordsTheDoorSpeaks` is what replaced
      it, and it asserts REACHABILITY, which a grep over the header could never see

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

- [x] the door's methods are `camelCase` and its types `PascalCase`, Filament's split exactly
- [x] no door verb is elliptic: each says what it does and to what
- [x] a refusal carries its reason, which is `std::expected` rather than Filament's silence

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


## THE VERB TABLE, and every exception carries its reason

Filament is four objects -- `Engine`, `SwapChain`, `Renderer`, `View` -- where this door is ONE. That
is the source of most exceptions below and it is a deliberate difference rather than an oversight: a
client here declares a scenario and asks for a frame, and does not assemble a render graph.

| this door | Filament / Cesium | verdict |
|---|---|---|
| `render(Extent)` | `Renderer::render(View*)` | FILAMENT'S. The view is the active one rather than an argument, because views are DECLARED here and named |
| `scene()` -> `Scene` | `Scene` | FILAMENT'S, and it was `Store` until this round |
| `readPixels(vector)` | `Renderer::readPixels` | FILAMENT'S |
| `Camera` / `View` | `Camera` / `View` | FILAMENT'S split |
| `Material` | `Material` / `MaterialInstance` | FILAMENT'S first half. We have no instance layer yet, and until a material is shared between subjects there is nothing for one to instance |
| `Lighting::IndirectLight[3]` | `IndirectLight` | FILAMENT'S, this round. It was `Environment`, which is nobody's word |
| `Lighting::Key` | `LightManager` directional | OURS, and the reason is that a `LightManager` manages MANY lights on entities; one declared key light needs no manager. When a scenario declares a second light this becomes Filament's |
| `Standing.GlobeAnchor` etc | `GlobeAnchor`, `LongitudeLatitudeHeight` | CESIUM'S |
| `SamplesHeight` | `sampleHeight` | CESIUM'S, spelled as this door spells a field |
| `loadProgress()` | `Cesium3DTileset::ComputeLoadProgress` | CESIUM'S |
| `drawsInto(window)` / `(extent)` | `Engine::createSwapChain` | OURS. Filament splits Engine, SwapChain and Renderer into three objects; naming this `createSwapChain` would promise an object model this door does not have |
| `setView(name)` | pass a `View*` to `render` | OURS. A view is DECLARED and named here, so it is selected by its name rather than held by a client |
| `preload(patienceS)` / `settled()` | `Renderer::flushAndWait` | OURS. Filament waits for the GPU; this waits for the EARTH, which is a different thing to name |
| `declare` / `assemble` / `advance` / `run` / `park` / `resume` / `save` / `restore` / `discard` | none | OURS. Filament is a renderer and has no scenario and no simulation |
| `setSurfaces` / `offers(Host*)` / `mix` / `measures` / `unacted` / `inspect` | none | OURS. Overlay, host, audio and instrumentation are outside a renderer's vocabulary |

## What the item still owes

- [x] all twelve door words are SPOKEN and each is REACHABLE, proven by
      `test/outshine/door/ScoreWhichWordsTheDoorSpeaks` with two detector controls. `Renderer`,
      `MaterialInstance` and `TransformManager` landed: the last two existed already as an unnamed
      `int` and as a capability with ZERO callers


## THE NAME COUNT, measured in `include/` rather than claimed

    Engine                    11        Renderer                   0
    Scene                     14        MaterialInstance           0
    View                       3        TransformManager           0
    Camera                     2
    Material                   6
    GlobeAnchor                1
    IndirectLight              1
    LongitudeLatitudeHeight    2
    Georeference               2
    sampleHeight               1

NINE OF TWELVE STAND. The three at zero are the same exception written three ways and it is one
sentence: **Filament is four objects where this door is one.** `Renderer` is a separate object there
that a client binds to a `SwapChain` and a `View`; here `Engine` renders, and adding the name without
the object would promise a model that does not exist. `MaterialInstance` has nothing to instance
until a material is shared between subjects. `TransformManager` manages transforms on entities, and
placements here live on the declaration.

TWO LANDED THIS ROUND, and both are Cesium's word for something the door already had loose:

- `LongitudeLatitudeHeight` -- longitude, latitude and height were three unrelated doubles inside
  `Standing`. 61 sites across 11 files
- `Georeference` -- `WorldSettings` fused a place on the Earth with gravity, air density, wind and a
  streaming patience. Only the place is a georeference, and separating it is what makes the name
  honest rather than decorative. 40 sites across 16 files


## THE THREE THAT DO NOT STAND, AND WHAT WOULD MAKE EACH LAND

The exception is not "we chose not to" -- each has a condition, and when it is met the name lands.
Left open so a later round can act rather than re-argue.

**`Renderer`.** Filament's owns the frame loop, and a client writes
`renderer->render(view)`. Here `Engine::render` does it. Adding the object would make every call
`engine.renderer().render(...)`, and THIS ITEM'S OWN MEASUREMENT is whether a client writes LESS --
`apps/demo` fell 194 to 170 through the door work. A `Renderer` that only lengthens every render
call fails that measurement, so the exception stands on the item's own number rather than on taste.
**It lands when there is a second render target**, because then a client genuinely has to say which
one, and the object stops being ceremony.

**`MaterialInstance`.** Filament's is a per-object override of a shared material. Every surface here
carries its own `Material`, so there is nothing shared to instance and nothing to override.
**It lands when two subjects share one material** and one of them needs to differ.

**`TransformManager`.** Filament's moves transforms on entities at runtime. Here a placement is
DECLARED and moving a thing is a re-declaration -- which is the engine's own invariant, not an
oversight. **It lands if a client is ever given a runtime handle to move one placed thing**, and
until then the name would promise a mutability the declaration model refuses.

## What the item still owes

- [x] the line-count premise, withdrawn above with its replacement measurement
