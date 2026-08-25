# Outshine

**A modern game engine combining the best of RAGE and Unreal.** Development platform IS the target:
Apple A18 Pro (2P+4E cores, 5 GPU cores, 8 GB, Metal 4), **720p60 held** — p50/p95/p99 over a moving
camera, never a mean.

- **SDL3 · SDL3_GPU · SDL3_\*** are the only platform surface; **glTF 2.0** the only content surface
- **C++23**, `-Wall -Werror -Wpedantic`, one `-std` for the whole tree; `static_assert` and the type system over checkers; `std::span`/`std::string_view` at boundaries, `std::mdspan` for field and instance views, `std::expected` where a refusal carries its reason
- **Precision has ONE boundary and it is the camera**: the scene keeps 64-bit positions and the
  renderer is camera-relative in 32-bit — `Anchor - Eye` in `double`, the model-view-projection
  product in `double`, and the cast to `float` only at the uniform push
  (`src/render/stages/SubjectDraw.cpp:841,846,854`). A `float` that ever holds a world position
  is a defect; a `double` that reaches a shader is a different one
- **SIMD- and optimization-friendly**: contiguous, one-width, pointer-free layouts; fast path on the hot path; batch over per-item; bounded terms on the frame path (no alloc/lock/disk/unbounded block)
- **Declarative**: scenarios declare, the engine behaves; content = data, engine = verbs; the consumer selects from a `constexpr` catalogue and cannot add to it
- **Batteries as declarations**: outshine ships convenience components -- generators, providers, world templates and factories (`Planet(params)` → a Scenario value) -- all catalogue citizens the scenario selects; the engine core stays scenario-agnostic
- **Every number carries its origin** (derived · measured · `[SET]`) with unit and population; no magic numbers; calibration measures, never decides
- **The code carries NO comments** — `src/`, `include/`, `tools/`, `apps/` hold no `//`, no block, no
  TODO, no derivation, no board number: names and structure carry the meaning, a number's
  origin lives in its board item and its commit, and prose may stand in a PROOF because a proof
  explains what it proves -- a proof being any source that carries `Covers("`, wherever it
  lives, and every source that does not being bound; work items live in `board/`, code never
  names them
- **Infrastructure built from OSM is PLAUSIBLE and geometrically correct, never necessarily
  true to the real road**: the data is a source of shape, not a specification to be reproduced,
  so a corner the graph demands is a finding only where it is implausible or geometrically
  wrong — never merely because the real road there is built to another class. And OSM does not
  carry the third dimension: **bridges, ramps, over- and underpasses, tunnels and every other
  3D course are RECONSTRUCTED**, and what a reconstruction owes is four kinds of plausibility —
  **geometric** (it closes, it is continuous, it does not intersect itself or what it crosses),
  **physical** (a vehicle can drive it at the speed the class implies), **static** (it stands:
  spans, piers and clearances that could carry their own load), **architectural** (it looks
  like the thing it is). A guess that holds all four is right; one that holds three is a finding
- **Cycles is the oracle** for correctness; references are for ambition; the corpus is a driver, not a certificate
- **One world space**; a failure is loud; something is always drawn; delete on the day you replace
- Artefacts go to the system temp dir, never the tree; `git log` is what was — no journal

**Diagram colours** — CURRENT: green = correct by current knowledge · amber = uncertain · red =
provably wrong · grey dashed = absent. TARGET: green = certain · amber = probable.

## Architecture (TARGET)

```mermaid
flowchart TD
  upstream["upstream — OSM · terrain · imagery · weather · sky"]
  providers["PROVIDERS · src/data"]
  store[("CONTENT STORE — hash = filename")]
  field["GROUND — the declared sphere's surface fields: height · slope · class · edges · water; one stack PER sphere, empty fields allowed, absent in free flight"]
  gen["GENERATORS — one part + capability, from (kind, params, seed, budget)"]
  comp["COMPOSITORS — one draw list: places · culls · quantises · batches"]
  rend["RENDERER — pixels from a declared plan"]
  frame(["720p60 on this device"])
  scen[/"SCENARIOS — camera × clock × world-or-studio"/]

  actors["ACTOR CHAIN — bodies · minds · presence, assembled from the scene store"]

  upstream --> providers --> store --> field --> gen
  gen -->|part| store -->|handle| comp -->|draw list| rend --> frame
  field --> actors -->|placements| comp
  scen -.->|declares| gen & comp & rend
  scen -.->|declares · clocks| actors
  tmpl[/"WORLD TEMPLATES — earth · moon: shipped declarations in src/assets"/] -.->|instanced, then deltas| scen
```

| layer may not spell | |
|---|---|
| any of src/ | Earth · Moon · a planet's name or numbers — worlds are declared spheres, templates are assets |
| Ground | camera · frustum · clock · LOD level · device · sun |
| generator | camera · neighbour part · draw list · device |
| compositor | device · pipeline · texture · shader · pass |
| renderer | any content noun |

Peers never call each other; a part-on-part dependency travels as data through Ground.
**The engine knows no Earth, no Moon, no stars — and GROUND is no planet either**: it is the
surface-field stack of whichever sphere is declared, instantiated per sphere, its fields empty
where the sphere has nothing (lunar water), and absent entirely for an actor in free flight —
the chain never requires a surface. A scenario declares a SYSTEM of spheres
(radius, gravity, providers, sky) shaped by height data; travel between them is an actor with
thrust and a possession relink, and local behaviour — the high jump, the bad driving — emerges
from the declared gravity through the physics, never from code. Earth ships as a TEMPLATE
(src/assets/scenarios/earth.xml, to be): a scenario instances it (`<world template="earth">`, one
level deep) and overrides by delta — setting replaces, removal is named, omission keeps the
template's value, an orphaned override refuses loudly, and the reader walks template then
deltas through the SAME assembly API (no merger). `Scenario Earth()` is the code-side factory
returning that declaration; the template alone must hold 720p60. CURRENT departs from this —
g and Earth radii still sit in code — and the audit with its repayment is the board's.
Budget = screen-space error in px, quantised to a global ladder before it becomes a key;
key = `(kind, params, seed, rung)` value, no strings. Degrade on detail, refuse on existence.

### The actor chain (TARGET — the owner's causal decomposition, general)

```mermaid
flowchart TD
  B["BODY — geometry, glTF parts: vehicle · walker · aircraft · door · pump"]
  B --> A["ACTUATORS — the functions a body declares: steer · drive · brake · lamps · walk · open"]
  A --> P["PHYSICS — forces at the contacts; only integration places a body"]
  C["CONTROLLER — a mind or the player POSSESSES the seam"] -->|acts on| A
  C -->|perceives| Q["PERCEPTION — bounded spatial queries: bounds · ground · sight"]
  C -->|asks| N["PATHFINDING — two coordinates in, corridor out: walk · drive · fly · rail"]
  PR["PRESENCE — field → rails → body; a MEASUREMENT materialises"] -.-> B
```

The chain holds for EVERYTHING that moves, with or without a mind: a parked car is a body whose
seam nobody possesses; a door is a body with one actuator; an NPC differs from the player only
in who possesses the seam (Unreal's Pawn/Controller possession — the reference). Perception is
spatial query over bounds, never privileged state; navigation is one pathfinding service with
modes, handed as a TOOL ; physics binds to actuators, never to the controller. Presence is a
rung on two axes (existence · fidelity, thresholds ordered): unmeasured actors are a conserving
FIELD, measured ones materialise (rails, then body) as a PURE EVALUATION of (kind, params, seed,
rung) — and materialisation is never transitive: minds read the abstract layer, only declared
instruments collapse it.
The drive is a product of free systems (AssembleDrive · DriveTick), no orchestration class left. **A client includes nothing but
`include/outshine/`** — enforced by the build.

### The component model (TARGET — the decided reference design)

Reference: **Flecs** (relationships + traits + script parity), supplemented by GAS tag algebra,
Smart-Object slots, CARLA's vehicle grammar. Written here, never a dependency.

```mermaid
flowchart TD
  XML["scenario XML"] --> API["ONE assembly API — same calls, same refusal text"]
  CPP["client C++"] --> API
  API --> STORE["entity store — ids and typed pairs (relation, target), values not pointers"]
  PRE["prefab / IsA — 'glTF as four-wheel', variants, named slots"] --> STORE
  STORE --> CAN["CAN — capability tags, constexpr catalogue (typo = compile error)"]
  STORE --> MAY["MAY — traits on the relation refuse at ASSEMBLY;<br/>situational permission is tag set-algebra at runtime"]
  STORE --> ACT["INTERACTS — world objects advertise slots as data; Free → Claimed → Occupied → Free"]
```

The XML reader is a serialisation of the assembly API against ONE graph — no second
representation, no converter (the Unity-baking rot). XML declares what the API can and nothing of
its own: no conditions, no control flow — behaviour belongs to the mind and the assignment. The
render plan stays a `constexpr` catalogue (unspellable beats refused-at-load); this graph is the
SCENE domain: vehicles, minds, tools, assignments, world objects. Banned by the sources
themselves: stringly-typed capabilities · content that ships a program · god actors ·
ECS-for-everything · unreserved shared affordances.

## Render plan (CURRENT — what executes, judged as architecture)

```mermaid
flowchart TD
  subgraph compute
    direction TB
    T["mediumTransmittance"] --> M["mediumMultiScatter"] --> R["mediumRadiance"]
  end
  subgraph raster
    direction TB
    LV["lightVisibility"] --> SUBJ
    SKY["sky"] --> HDR[("SceneHdr")]
    SUBJ["subjects"] --> HDR
    GLASS["subjectsTransmissive — a cloned stage"] --> CT["compositeTransmission"]
    HDR --> TAA["temporalResolve — encodes nothing, folded into tonemap"] --> TONE["tonemap"]
    TONE --> OV["overlay"] --> P["present"]
  end
  R --> SKY

  classDef sound fill:#1f6f3f,stroke:#0d3b21,color:#fff
  classDef unsure fill:#8a6d1f,stroke:#4a3a0d,color:#fff
  classDef wrong fill:#7a2222,stroke:#3d1111,color:#fff
  class T,M,R,SKY,LV,CT,TONE,OV,P sound
  class TAA unsure
  class SUBJ,GLASS wrong
```

Green = sound abstraction by current knowledge; amber = form in question; red = provably wrong,
and each red cites what makes it so:

| red | what makes it red, at HEAD |
|---|---|
| `SUBJ` | one stage carrying six responsibilities -- `ShaderSource(const SourceOptions &options)` (SubjectDraw.h:30), `PipelineAt(VertexLayout layout, SurfaceKind kind, bool cullsBack)` (:154), `FlushCrossings(SDL_GPUCommandBuffer *commands)` (:148), `SetPlacements(const double *models, size_t rows, std::string &error)` (:51), `SetLights(std::span<const SubjectLight> lights, std::string &error)` (:89) and `EncodeDepthOnly(const double lightFromWorld16[16], const double eye[3], int atlasPx,` (:96) beside its one `void Encode(const FrameContext &ctx, const PassRecording &into)` (:93); nothing culls |
| `GLASS` | `{Stage::SubjectsTransmissive, Provenance::Content, PassKind::Raster, "subjectsTransmissive",` (RenderCatalogue.h:268) is a full clone of `{Stage::Subjects, Provenance::Content, PassKind::Raster, "subjects",` (:263) -- transmissive draws belong in the one subject stage |

One `Writes` producer per derived resource (`static_assert`); missing contributor = picture
choice, **published** as `-> neutral`; load/store ops derived from the plan (`Stored()`).

## Render plan (TARGET)

```mermaid
flowchart TD
  subgraph compute
    direction TB
    T2["mediumTransmittance"] --> M2["mediumMultiScatter"] --> R2["mediumRadiance"]
    R2 --> IR["irradiance"] --> AE["autoExposure"]
  end
  subgraph raster
    direction TB
    LV2["lightVisibility"] --> GEO
    SKY2["sky"] --> HDR2[("SceneHdr")]
    SUN["sun"] & MOON["moon"] & STARS["stars"] --> HDR2
    GEO["terrain · buildings · water · models"] --> HDR2
    SUBJ2["subjects — resident, culled, instanced"] --> HDR2
    GLASS2["transmissive draws in the one subject stage"] --> CT2["compositeTransmission"]
    AO["ambientOcclusion"] --> HDR2
    HDR2 --> TAA2["temporalResolve"] --> TONE2["tonemap"] --> OV2["overlay"] --> P2["present"]
  end
  R2 --> SKY2

  classDef sure fill:#1f6f3f,stroke:#0d3b21,color:#fff
  classDef likely fill:#8a6d1f,stroke:#4a3a0d,color:#fff
  class T2,M2,R2,LV2,SKY2,SUBJ2,CT2,TONE2,OV2,P2,GEO,SUN,MOON,STARS,IR sure
  class AE,AO,TAA2,GLASS2 likely
```

Green = certain; amber = probable (auto-exposure shape, AO method, TAA's place, and whether
transmissive draws fold into the subject stage are open design calls).

## Class structure (CURRENT)

```mermaid
flowchart TD
  Transport --> WebTileSource --> ContentStore --> TerrariumDem & VersatilesVector
  TerrariumDem --> GroundStream
  TilePool --> GroundStream & OsmField
  GroundStream --> GroundQuery["GroundQuery — the two questions: At · PostM"]
  GroundQuery --> CorridorLay & WaterField & BuildingField
  VersatilesVector --> OsmField --> RoadHarvest --> Wayfinding
  OsmField --> StreetField & BuildingField & WaterField
  GroundStream --> Ground --> Forest & Buildings & Water & Infrastructure
  Wayfinding --> Alignment["Alignment — one arc per RUN of same-sign turns, the accuracy bound splits it"] --> ReferenceLine --> Carriageway --> Ribbon
  Carriageway --> SpeedProfile --> Pilot --> Walk & Drive & Fly & Rail
  Drive --> Rig --> Body
  Rig --> Contact & Shear
  Forest & Buildings & Water & Ribbon & Subject --> DrawList
  DrawList --> SubjectDraw --> Renderer
  SubjectResidency["SubjectResidency — buffers · staging · BVH · textures"] --> SubjectDraw
  MediumTransmittanceStage --> MediumMultiScatterStage --> MediumRadianceStage --> SkyStage --> Renderer
  LightVisibilityStage --> Renderer --> TonemapStage --> PresentStage
  Live --> Renderer
  Ephemeris & RegionForge --> Sim --> Renderer
  Frustum -.-> DrawList
  TilePool --> World["World — quadtree LOD · admission · kerbs"] --> Sim
  GroundStack["GroundStack — owns store · sources · pool · stream"] --> DriveAssembly["AssembleDrive — scene handles + ground → DriveProduct"]
  GroundStream --> DriveAssembly
  DriveAssembly --> CorridorLay["CorridorLay — route + ground → Corridor product"]
  DriveAssembly --> DriveTick["DriveTick — (Corridor, Rigged, DriveState) tick"]
  Engine --> Live
  Markup --> Stylesheet --> LayoutUi["Layout"] --> Painting --> OverlayDraw --> Renderer
  GltfStudio --> Renderer
  Assembly["Assembly — the XML door"] --> SceneStore["Scene Store — entities · typed pairs · traits · tags"]
  InputMap["InputMap — declared bindings, interned to ids"] --> InputPump["InputPump — SDL events to (action id, kind, value)"]
  TriggerField["TriggerField — volumes fire declared events: enter · exit · dwell"]
  ViewBook["ViewBook — one active view: follows · clock scale · the ear"]
  BusGraph["BusGraph — the mix: buses into buses, one master, falloff per source"]

  classDef sound fill:#1f6f3f,stroke:#0d3b21,color:#fff
  classDef unsure fill:#8a6d1f,stroke:#4a3a0d,color:#fff
  classDef wrong fill:#7a2222,stroke:#3d1111,color:#fff
  classDef strandedSound fill:#1f6f3f,stroke:#7a2222,stroke-width:3px,stroke-dasharray:6 4,color:#fff
  classDef strandedUnsure fill:#8a6d1f,stroke:#7a2222,stroke-width:3px,stroke-dasharray:6 4,color:#fff
  class Transport,WebTileSource,ContentStore,TerrariumDem,VersatilesVector,GroundStream,GroundQuery,OsmField,RoadHarvest,Wayfinding,Alignment,StreetField,Ground,ReferenceLine,Carriageway,Ribbon,SpeedProfile,Pilot,Walk,Drive,Fly,Rail,Rig,Body,Contact,Shear,MediumTransmittanceStage,MediumMultiScatterStage,MediumRadianceStage,SkyStage,PresentStage,SceneStore,Assembly,SubjectResidency,Markup,Stylesheet,LayoutUi,Painting,InputMap,InputPump,TriggerField,ViewBook,BusGraph sound
  class BuildingField,WaterField,Subject,DrawList,Renderer,TonemapStage,LightVisibilityStage,Frustum,Ephemeris,GltfStudio,Engine unsure
  class World,SubjectDraw,Sim,Live wrong
  class DriveAssembly,CorridorLay,DriveTick,TilePool unsure
  class Forest,Buildings,Water,Infrastructure strandedSound
  class RegionForge strandedUnsure
  class GroundStack sound
```

Colours are ARCHITECTURE, re-adjudicated against HEAD (2026-08-24); every red below cites what
makes it red, and a colour whose stated reason has gone stale is itself a finding. Green = right
responsibility in the right layer; amber = form in question. **A DASHED RED OUTLINE is the
second axis, added 2026-08-24 (board:1805): the node's FILL says whether its shape is right,
the outline says whether anything outside `src/` can reach it — a dashed node's only path to a
client runs through a node this map colours red, so its fill is a judgement about a layer
nobody calls.** Green and reached is the only state that means the picture gets drawn.

| red | what makes it red, at HEAD |
|---|---|
| `World` | spells camera and LOD inside the ground layer: `struct Eye` (World.h:49), `Refine(const Eye &eye, double nowMs)` (:55), `EyeInMercatorBand()` (:118), and 9 `const double eye[3]` (:189-195) |
| `SubjectDraw` | six responsibilities in one class: `ShaderSource(const SourceOptions &options)` (SubjectDraw.h:30), `PipelineAt(VertexLayout layout, SurfaceKind kind, bool cullsBack)` (:154), `FlushCrossings(SDL_GPUCommandBuffer *commands)` (:148), `SetPlacements(const double *models, size_t rows, std::string &error)` (:51), `SetLights(std::span<const SubjectLight> lights, std::string &error)` (:89), and `EncodeDepthOnly(const double lightFromWorld16[16], const double eye[3], int atlasPx,` (:96) beside the one `void Encode(const FrameContext &ctx, const PassRecording &into)` (:93) a stage owes |
| `Sim` | `class Sim {` (Sim.h:37) is a hand-wired god facade the component model replaces: a facade reached through 25 `#include "` |
| `Live` | `class Live {` (Live.h:71) reaches the renderer and the layout from one class — `#include "Renderer.h"` (:18) beside `#include "Layout.h"` (:13) |

Amber says the FORM is in question, and every amber below names the line the question stands
at (board:1777):

| amber | the form in question, at HEAD |
|---|---|
| `BuildingField` | `class BuildingField {` (BuildingField.h:20) holds a `struct Footprint` of raw index ranges (`uint32_t FirstPoint = 0, PointCount = 0;`, :24) and takes a mesher by pointer (`void Shapes(const StructureMesher *mesher)`, :32) -- a field that tessellates |
| `WaterField` | `void Tessellate(const OsmField &field, std::vector<float> &out) const;` (WaterField.h:47) -- the same: a field that meshes rather than one that answers |
| `Subject` | `class Subject {` (Subject.h:98) carries 42 `[[nodiscard]]` over one glTF document -- the getter carpet |
| `DrawList` | `class DrawList {` (DrawList.h:167) with `struct VertexLayoutRow {` (:49) beside it: the list and the layout table in one header |
| `Renderer` | `class Renderer {` (Renderer.h:34) publishes 54 `[[nodiscard]]` and 15 `const {` -- the getter carpet, on the frame path |
| `TonemapStage` | `class TonemapStage {` (TonemapStage.h:14) is where `temporalResolve` folded into, so it carries two picture decisions |
| `LightVisibilityStage` | `class LightVisibilityStage {` (LightVisibilityStage.h:16) -- one shadow atlas for every light, no cascade selection declared |
| `Frustum` | `struct Frustum {` (Camera.h:94) sits in core beside the camera, while culling belongs to the compositor |
| `Ephemeris` | `inline void EarthSunPos(double lat, double lon, double utc, float *el, float *az) {` (Ephemeris.h:11) -- a whole-function header, out-parameters by pointer, and `constexpr int kEphemerisMinYear = 1901, kEphemerisMaxYear = 2099;` (:9) bounding a sphere the engine may not name |
| `RegionForge` | `class RegionForge {` (RegionForge.h:18) forges regions from a client layer |
| `GltfStudio` | `struct Studio {` (GltfStudio.h:26) beside `struct StudioScratch {` (:49) -- the studio and its scratch are two spellings of one stand-up |
| `DriveAssembly` | `[[nodiscard]] bool AssembleDrive(const Store &scene, const Assembled &cast,` (DriveAssembly.h:60) takes a product's worth of inputs, listed one per line, and a product that needs that many is a product whose shape is not settled |
| `CorridorLay` | `[[nodiscard]] bool LayCorridor(const Path::Route &route, const GroundQuery &ground,` (CorridorLay.h:100) -- same shape, same question. The product it lays holds no band parallel to its stations: `std::vector<Station> Fine;` (:52) is one extent and `At(double alongM)` (:68) one clamped index, so an unlaid corridor refuses at the tick's entry instead of returning a default per read (board:1820) |
| `DriveTick` | `[[nodiscard]] const Ridden &DriveTick(const Corridor &way, const Rigged &stood,` (DriveTick.h:111) hands back the accumulator the caller owns -- `Ridden &out = drive.Tally;` (DriveTick.cpp:38). The copy is gone and the struct is 2472 -> **440 bytes, which `static_assert(sizeof(Ridden) == 440)` at DriveTick.cpp:18 is the measurement of**; what stays in question is a product that is both a per-tick answer and a route-long tally (board:1815) |
| `TAA` | `{Stage::TemporalResolve, Provenance::Content, PassKind::Raster, "temporalResolve",` (RenderCatalogue.h:278) declares a stage that encodes nothing of its own -- it is folded into tonemap rather than standing as its own resolve |
| `TilePool` | `class TilePool {` (TilePool.h:35) holds 3 `std::mutex`, a `std::condition_variable`, a `std::map` and a `std::set` where a slot table and a ring would do -- a decisionless pool holds no tree |
| `Engine` | the ONE door swallows half its declaration: `bool Engine::Assemble() {` (Engine.cpp:101) reads the scenario's `Drive Driven` and never reaches the drive -- `grep -rn AssembleDrive src` finds only `DriveAssembly.{h,cpp}` themselves, so every call site of `AssembleDrive` in the tree is a TEST. The driver's own entry point runs Read -> Declare -> Assemble -> RenderTo -> Advance and gets a studio orbit of a car on white. Green on shape, amber because a door that ACCEPTS a declaration it does not execute is a worse door than one that refuses it (2e779901's own words) |

Dashed says the node is STRANDED, and every dashed node below names the facade it hangs off
(board:1805, measured 2026-08-24):

| stranded | its only way to a client, at HEAD |
|---|---|
| `Forest` | `src/clients/Sim.{h,cpp}` and nothing else in `src/` |
| `Buildings` | `Sim.{h,cpp}`, `BuildingField.cpp`, `OsmLayer.h`, `World.{h,cpp}` -- every one of them inside the ground/client pair |
| `Water` | `Sim.cpp`, `World.h` |
| `Infrastructure` | `Sim.h` |
| `RegionForge` | `Sim.h` |

and `Sim` itself: `grep -rln '"Sim.h"' src test tools apps` finds `src/clients/Sim.cpp` and one
test. 798 lines of facade, no consumer. The world composition path exists, is proven per node
since board:1806, and nothing walks it -- which is why `apps/driver` builds its terrain, its
far ring and its road ribbon in 1281 lines of its own C++.

`TilePool` moved red → amber (its row above carries the form now in question): the earlier
sentence said it spells camera and LOD, and it does not — `grep -cEi 'eye|camera|frustum|\blod\b'` over both its files is 0. It is a
byte-budgeted LRU work pool keyed on projected error, which is the RAGE reference, not a
layering breach. Its amber is its FORM, and its row above carries the count: a `condition_variable`, a `std::map`
and a `std::set` of pointer-chasing nodes where a slot table and a ring would do — a
decisionless pool holds no tree. `LayCorridor`'s twin debt is PAID (board:1624 closed
2026-08-24): its door takes `const GroundQuery &` -- the two questions it asks -- and
`test/unit/sim/ACorridorIsLaidOverASyntheticRoute` lays 364 lines of route over a ten-line
synthetic ground, so its amber is now the parameter list alone, as its row says.
`AssembleDrive` and `DriveTick` keep the older reason. Journey died with move 2(e): the six consumers hold
{GroundStack, DriveProduct} and call the free systems directly. The rot concentrates at the
orchestration edges; the middle of the tree is sound.

## The distance to TARGET (measured 2026-08-25, by the hourly review)

Green AND reached is the only state that draws a pixel: a green node whose only path to a client
runs through a red one is counted separately, because its fill is a judgement about a layer
nobody calls.

| diagram | green | amber | red | stranded | total | green-and-reached | last hour |
|---|---|---|---|---|---|---|---|
| class structure (CURRENT) | 44 | 15 | 4 | 5 | 68 | **44 / 68 = 65 %** | 44 / 67 = 66 % |
| render plan (CURRENT) | 9 | 1 | 2 | 0 | 12 | **9 / 12 = 75 %** | 9 / 12 = 75 % |

**The distance GREW this hour: 66 % -> 65 %.** Two moves, in opposite directions and both
measured:

| move | why |
|---|---|
| `Alignment` ADDED, green and reached | src/actor/path/Alignment.{h,cpp} landed; reached through `Fit`, called at `fitted = Fit(keptM, quantumM, tightestM, classTightestM, corridor);` (CorridorLay.cpp:70). Numerator and denominator both +1, so the share alone would have stood still |
| `Engine` green -> AMBER | `2e779901` measured what its own entry point does: `Assemble` accepts `Scenario::Driven` and never drives it. The one public door does not reach the drive, and the row above carries the citation |

No red went amber; no amber went green. What the hour bought that the table cannot show is a
TARGET node reaching its mark: `Alignment` in the class-structure TARGET is now GREEN, because
the split rule that made it amber is settled and measured -- the accuracy bound ends a run, not
the sign change, at 1 bend within 40 m, 2 within 8 m, 4 within 1 m, worst departure inside the
bound every time (`test/unit/actor/path/AnAlignmentIsTheArcTheVerticesDescribe`).

What blocked the rest: `test/run.sh` runs NOTHING at HEAD (board:1860) -- the walk over `apps/`
takes the driver's new entry point for a test case and dies with exit 2 before the library is
built -- and the driver's only eye needs 358 s where the gate's clock allows 120 (board:1861).
An hour whose gate is down cannot turn a node any colour.

The four reds carry the distance: `World` (camera and LOD inside the ground layer), `SubjectDraw`
(six responsibilities in one stage), `Sim` (a hand-wired god facade with no consumer), `Live`
(renderer and layout from one class). Five stranded nodes hang off `Sim` alone.

## Driver (the product the engine is judged by)

The bar is Gran Turismo 7 on PS4 at 720p60 on the A18 Pro (board:1573). What the app can show
today, each row naming what proves it:

| | stands | proven by |
|---|---|---|
| a route from two coordinates | yes | `apps/driver/test/APlannerFindsTheRoadFromMunichToHamburg` |
| a road ribbon the wheels stand on | yes | `apps/driver/test/TheRoadEdgeIsContinuousWhereSegmentsMeet` |
| terrain with real relief under the road | yes | `$TMPDIR/outshine-stills/km0114.5-framed.png` -- rolling hills, an undulating horizon |
| a car that reads as the car it is | yes | the same still: body, glass, wheels, tail lights, a legible plate |
| stills along the drive | **written, never judged** | `apps/driver/test/stills/StillsAreTakenAlongTheDriveForTheEye` is killed at the gate's 120 s clock with a 0-byte log (board:1861) |
| a window that shows the drive | yes | `apps/driver/test/window/AWindowShowsTheRoadTheCarIsDriving` |
| an entry point -- a program a user runs | **written, runs nowhere** | `apps/driver/src/main.cpp` exists at 146 lines and links against `include/outshine/` alone; it takes the whole gate down (board:1860) and no working directory satisfies it (board:1859) |
| the sun's direction readable on the ground | **NO** | `km0114.5-framed.png` -- two hills facing apart carry the same value (board:1567) |
| the car's shadow on the road | **NO** | `km0721.0-third.png` -- the deck under the car is the deck's own grey (board:1575, box 3) |
| a road edge without a bite | **NO** | `km0114.5-framed.png` x 900..1160 and `km0016.8-framed.png` x 600..1050 -- sawtooth verge into deck (board:1568) |
| nothing floating at the horizon | **NO** | `km0114.5-first.png` -- two navy slivers above the terrain silhouette (board:1565) |
| road markings, guard rails, verge furniture | **NO** | nothing declares them |
| a second carriageway on a motorway | **NO** | every still shows ONE deck and no central reservation |
| a ground material with texture | **NO** | every still: one flat albedo for the verge, one for the deck, no detail at any range |
| buildings behind the verge, drawn | declared, not reached | `Buildings` is stranded off `Sim` |
| culling and instancing | **NO** | board:1538 -- every subject is drawn every frame |

The gap to the bar, named rather than gestured at: GT7 on PS4 puts a cascaded shadow under the
car, a grazing-angle specular sheet on the asphalt that carries the sun, lane and edge markings
with tar seams, a verge with guard rail, poles and vegetation, and aerial perspective that fades
the far ground into the sky. The stills carry NONE of those five. What they do carry -- a
correct camera, a sound sky model, a body that reads, terrain that undulates -- is the half of
the frame the engine already owns.

## Class structure (TARGET — where the tree is going)

```mermaid
flowchart TD
  Scenario["Scenario XML"] --> Assembly
  ClientCode["client C++"] --> Assembly
  Assembly --> SceneStore["Scene Store — entities · pairs · traits · tags · slots"]
  SceneStore --> Columns["Columns — vehicle numbers · placements, by handle"]
  SceneStore --> SimD["Sim — owns the drive: corridor · speed plan · pilot"]
  SimD --> Pathfinding["Pathfinding tool — walk · drive · fly · rail"]
  Pathfinding --> Alignment["Alignment — one arc per RUN of same-sign turns; a transition only where curvature reverses"]
  Alignment --> Line["ReferenceLine — the corridor the wheels stand on"]
  SimD --> Physics["Rig · Body · Contact — forces at the patch"]
  SimD --> WorldC["World composition — the scenario declares a sphere, the engine composes its fields"]
  WorldC --> Compositors["Compositors — terrain · ring · cut-fill placement"]
  Line --> Compositors
  Compositors --> DrawList --> Registry["stage registry — the executor table"]
  Registry --> Stages["stages: source · residency · encode split"]
  Stages --> Frame(["720p60"])
  Entities["entity store + culling"] -.-> DrawList

  classDef sure fill:#1f6f3f,stroke:#0d3b21,color:#fff
  classDef likely fill:#8a6d1f,stroke:#4a3a0d,color:#fff
  class Scenario,ClientCode,Assembly,SceneStore,SimD,Pathfinding,Physics,Registry,DrawList,Frame,WorldC,Line,Alignment sure
  class Columns,Compositors,Stages,Entities likely
```

Two nodes were new on 2026-08-24 and each is argued rather than wished:

- **`Alignment` replaces the per-vertex corner table**, and the reason is a measurement, not a
  preference. `Fit` put a spiral-arc-spiral at EVERY vertex and returned the curvature to zero
  between them, so a polyline that describes a curve was laid at `R/(1+alpha)` — `alpha = 0.5` in
  the code, giving exactly two thirds of the true radius, reproduced to four digits over five
  values of alpha and at every digitisation density (board:1795). No constant repaired it; a
  transition is owed where the CURVATURE changes, not where a digitiser put a point. On
  Munich--Hamburg 769 of 2202 corners (35 %) sit inside a run of same-sign turns.
  **It goes GREEN 2026-08-25**, by the review, and the thing that lifts it is the split rule
  that made it amber: the ACCURACY BOUND ends a run, not the sign change. A tightening spiral
  turns one way throughout, so the bound alone says where one arc becomes two — 1 bend within
  40 m, 2 within 8 m, 4 within 1 m, worst departure inside the bound at every one
  (`test/unit/actor/path/AnAlignmentIsTheArcTheVerticesDescribe`). A 400 m circle now fits at
  400.000 m at 10, 20, 50 and 100 m chords where the corner table laid 266.6 m at every one
  (`test/unit/actor/path/ACurveIsFittedAtTheRadiusItHas`, its `EXPECT_FAIL` declaration removed
  from test/run.sh:41). The node stands in CURRENT as well, green and reached through
  `Fit`, called at `fitted = Fit(keptM, quantumM, tightestM, classTightestM, corridor);`
  (CorridorLay.cpp:70).
- **`World composition` is the layer board:1805 found missing**, and it is green because the
  requirement is not in doubt: *scenarios declare, the engine behaves*. A scenario declares the
  sphere and which surface fields it wants drawn; the engine composes them. That the one client
  in the tree builds its own terrain and ribbon in C++ is the distance to be repaid, not a
  reason to lower the target.

## Public interface (CURRENT)

```mermaid
classDiagram
  direction TB
  class Engine {
    +Read(path) bool
    +Load(path) bool
    +Declare(scenario) bool
    +Declared() Scenario
    +Carried() strings
    +Assemble() bool
    +Scene() Store
    +Advance() bool
    +Run() bool
    +Park() / Resume(name) / Discard(name) / Parked()
    +Save(path) / Restore(path)
    +RenderTo(frame)
  }
  class Live {
    +Open(renderer, declaration) bool
    +Restand(built) bool
    +Carry(body16, built16) bool
    +Eye(placement) / FrameItself()
    +SkyEye(aboveGroundM)
    +Advance() bool
    +Screenshot(path) bool
    +ReadPixels(rgba) bool
  }
  class Renderer {
    +Init(w, h, plan)
    +SetSubjectMesh/Pose/Placements/Materials/Lights
    +SetMedium(medium) / SetSky(sun, up, lux, eyeM)
    +SetShadowFrame(sun, up, radiusM) / ShadowCentre(m3)
    +SetCameraBasis(eye, fwd, right, up)
    +ShowOn(window) / ShowOffscreen(w, h) / PresentFrame() / StopShowing()
    +RenderFrame() / ReadPixels()
    +WhyNot() string
  }
  class GroundStream {
    +At(lat, lon) GroundSample
    +BlockAt(z, x, y) GroundBlock
    +PostM(latDeg) double
  }
  Engine --> Live : owns
  Live --> Renderer : drives
  Sim --> GroundStream : owns
```

`Live`, `Renderer`, `GroundStream` and the drive systems are still reachable by clients —
the TARGET below removes them. What a client hands the renderer is a SURFACE — a window or an
extent — and what comes back is
`std::expected<std::optional<Shown>, std::string_view>` (Renderer.h:50): an ABSENT `Shown` is
"no image this frame", which a minimised window gives and `SDL_gpu.h:4300` calls *"not an
error"*, while `std::unexpected` is reserved for the three faults — no surface declared, no
device, no command buffer — and every one of them is a `string_view` into static text, so the
present path allocates nothing. `SDL_ClaimWindowForGPUDevice`,
`SDL_WaitAndAcquireGPUSwapchainTexture` and `SDL_ReleaseWindowFromGPUDevice` have no call site
outside `src/render/` (`harness/claims/TheDeviceLeavesTheLibraryOnlyForItsOwnTwins`,
board:1826).
The assembly API stands public since 2026-08-22: `include/outshine/{Register,Store,Column}.h`
are the C++ door, `Engine::Assemble/Scene` own the one graph, proven by a client that includes
nothing but `outshine/`.

## Public interface (TARGET — one door)

```mermaid
classDiagram
  direction TB
  class Engine {
    +Read(path) bool
    +Declared() Scenario
    +Assemble() bool
    +Scene() Store
    +Advance() bool
    +RenderTo(frame) void
    +WhyNot() string
  }
  class Store {
    +Add(role) Entity
    +Give(entity, tag) bool
    +Link(from, relation, to) bool
    +Instantiate(prefab) Entity
    +Claim, Use, Release
    +queries over tags and pairs
  }
  class Scenario {
    +the declaration serialised against one graph
  }
  Engine --> Scenario : reads
  Engine --> Store : owns the one graph
```

**The engine carries no gameplay verb.** Driving is an assembly, not a method: body `IsA`
four-wheel, `DrivenBy` mind, mind `Uses` a nav tool, mind `Assigned` a route as data — declared
in XML or built through `Scene()`, which are the same calls against the same graph. A verb per
activity on `Engine` would leak the catalogue into the facade and hand XML a graph it cannot
spell. Possession is the `DrivenBy` relation — the player's mind and the autopilot take the same
seam, so "take the wheel" is one relink, not an API. A client includes `include/outshine/` and
nothing else; Live, Renderer, GroundStream and the drive fold behind this door.

## Tests

```mermaid
flowchart TD
  q{"what would fail it?"}
  q -->|wrong computation| u["test/unit — mirrors src/, IS the layering proof"]
  q -->|wrong pixels vs oracle| r["test/render — Cycles, per-vendor corpora"]
  q -->|wrong on device| s["render/outshine/shader — MSL vs C++ twin"]
  q -->|cost moved| f["render/outshine/frame — no sanitiser"]
  q -->|floor broke / drifted| c["render/outshine/scenario — p50/p95/p99 · determinism · memory"]
```

The unit mirror is the REGRESSION GATE and it is fast; the long device and corpus suites are the
sporadic full proof, run when named, never per edit. `test/run.sh` is the only runner (one process per test, real verdict, includes per layer = the
build's own sets). `tools/` and `apps/` build ON the library and run only by name. Oracle pipeline:
fetch → generate → patch (both sides) → convert → Cycles → compare (perceptual tail / geometric
bound, 0.005 px floor); **criteria met** and **cases within bound** published side by side.
Read the trailer first — a count without `N tests: … PASS … FAIL` may measure the past. The
fast gate also publishes **what it did not judge**: every source it stood aside from is still
COMPILED (`N source(s) the gate did not run still compile, M do not`, and `M > 0` turns the
gate red — board:1766), and every declared case family holding no fetched subject is named,
because a corpus is fetched and a green trailer must not read as coverage it never had
(board:1765). `test/run.sh --corpus` answers that second question alone. A case whose RED is a standing
finding is declared in `EXPECT_FAIL` with its failure count: the gate keeps its meaning, and
`run.sh` turns the gate RED the day that case passes with the declaration still in place, so a
repair cannot land quietly beside a stale expectation.
The one offline script: `test/harness/shared/corpus/prepare.py`. The corpus lives in the system
temp dir, so the machine may sweep it: a case whose prepared input is gone is **rebuilt from its
owning manifest** before it is judged — per owner, found by inverting the prepared-directory
mapping onto the path the failing case names in its log, its cost beside the bound with the
builds. A missing subject says `UNPREPARED`, never `FAIL`: the first says this run judged
nothing here, the second says the code is wrong (board:1797, 1798).

## Board

```mermaid
stateDiagram-v2
  [*] --> open : filed (defect found = item, same round)
  open --> active : being worked NOW (groom parent/depends)
  active --> closed : body names the proving test
  active --> open : parked
  closed --> open : Regresses/Supersedes
```

One file = RFC 822 header + markdown body. Fields: `Type` (feature|task|bug|issue) · `Parent`
(task→feature or task→issue: working a reviewer issue files a task attached to it) · `Area` (the tree's layers) · `Tags` · `Depends` · `Regresses` · `Supersedes`.
Filename `NNNN_label.md`; number = identity; no State/Id/dates — directory and git are the truth.
Titles say what WILL BE TRUE. Comments record what was LEARNED (append-only). Commits reference
`board:NNNN`. `board/active/` mirrors what is being worked on right now — always.

```sh
ls board/active/                                     # in flight NOW
grep -l '^Type: bug' board/open/*.md                 # by kind
grep -l '^Parent: 0007' board/*/*.md                 # a feature's children
git log --grep 'board:0042'                          # every commit on an item
ls board/*/ | grep -o '^[0-9]\{4\}' | sort -n | tail -1   # next id, derived
```

## Setup

| | |
|---|---|
| `src/` | the library entire; `src/assets/` its declared data; no entry point, no test |
| `test/` | `test/unit/` (mirror), `test/render/` (per vendor), `test/harness/` (scorers + `test/harness/claims/`) |
| `tools/` | development support built ON the library: what serves the PROCESS — transports, and a browser for looking at the corpus |
| `apps/` | applications built ON the library: what serves the PRODUCT — not tests, but they exercise the whole engine and are run by name; the engine knows none of them. **At HEAD `apps/driver` is split `src/` and `test/`, and `src/` holds two declarations (`f31.scenario`, `routes.xml`) and no program** — the shape is right and the entry point has not been written (board:1803) |
| `Makefile` | build · test · clean, nothing else |
| `board/` | the working system (above) |

## The hourly architect

A cron fires at **:17 every hour** and runs the `architecture-reviewer` agent over the tree:
it reads this file and the commit delta since the last review, judges the code as a principal
engineer would (RAGE and Unreal are the benchmark), and files what it finds into `board/`.
The review NEVER edits src/ — it files, sharpens and REOPENS; the repair is the queue's work.

| | |
|---|---|
| what it delivers | delta verdict · findings with file:line · new/changed board items · **what moved in CURRENT and in TARGET** · an explicit defect-free yes/no |
| where its findings land | `board/open/`, numbered from the next free id, one commit per round |
| a finding that reappears | reopens the item HARDER, with the measurement that disproves the closure |
| its gate | run only in its own `git worktree` — the main nest is pid-locked (`test/run.sh`) |

**It owns both maps, and the aim is CURRENT = TARGET** — the distance between them is the
work list. The review is the only JUDGE of the diagrams in this file; who may WRITE one depends
on whether the change is a measurement or a verdict (board:1855):

| | who writes it | why |
|---|---|---|
| a CURRENT node's existence, name, edges; a `file:line`, a count, a measured size | whoever moved the tree, the same session | a node that is gone is not an opinion, and CLAUDE.md must not lie about HEAD between two rounds |
| a CURRENT node's COLOUR and the row that argues it | the review alone | green/amber/red is a verdict on whether a shape is right, and a repair may not grade its own work |
| anything in TARGET | the review alone | where the tree is going is not decided by the item in flight |

`harness/claims/TheMapCitesLinesThatSayWhatItClaims` holds the first row to its word: every
`file:line` the CURRENT tables cite must carry the text quoted beside it.


| | | |
|---|---|---|
| **CURRENT** | the tree at HEAD, measured | MUST be corrected when the code moves — a node added, removed, renamed, recoloured, or a `file:line` citation that no longer says what its row claims |
| **TARGET** | where the tree is going | MAY change on a fetched reference, a measurement, or an owner requirement — argued in the commit, never silent |

No aspirational green; "it turned out harder" never lowers TARGET. A node that reaches its
target goes green and is named in the report. A gap no board item covers gets one filed.

Between reviews the open board is worked continuously: pick the next item, repair it, close
it with the proving test named in its body and a NEGATIVE CONTROL that shows the test red
against the defect. A closure that names no such test is not a closure. Done is: the board
holds no open item and a full round finds nothing — said explicitly — and the round after
it agrees.

References (fetched, not recalled): C++ Core Guidelines (binding) · Gregory GEA3 · RTR4 · PBR4 ·
Frostbite PBR/FrameGraph · Nanite (error-driven LOD) · Decima (runtime placement) · id Tech 7
(hard frame floor) · RAGE (decisionless pools) · SpeedTree (LOD cross-fade). Distance-ratio LOD
selection is refused by construction — one currency: projected error.
