# Outshine

**A modern game engine combining the best of RAGE and Unreal.** Development platform IS the target:
Apple A18 Pro (2P+4E cores, 5 GPU cores, 8 GB, Metal 4), **720p60 held** — p50/p95/p99 over a moving
camera, never a mean.

- **SDL3 · SDL3_GPU · SDL3_\*** are the only platform surface; **glTF 2.0** the only content surface
- **Modern C++**, `-Wall -Werror -Wpedantic`; `static_assert` and the type system over checkers
- **SIMD- and optimization-friendly**: contiguous, one-width, pointer-free layouts; fast path on the hot path; batch over per-item; bounded terms on the frame path (no alloc/lock/disk/unbounded block)
- **Declarative**: scenarios declare, the engine behaves; content = data, engine = verbs; the consumer selects from a `constexpr` catalogue and cannot add to it
- **Every number carries its origin** (derived · measured · `[SET]`) with unit and population; no magic numbers; calibration measures, never decides
- **The code carries no commentary**; work items live in `board/`, code never names them
- **Cycles is the oracle** for correctness; references are for ambition; the corpus is a driver, not a certificate
- **One world space**; a failure is loud; something is always drawn; delete on the day you replace
- Artefacts go to the system temp dir, never the tree; `git log` is what was — no journal

**Diagram colours** — IST: green = correct by current knowledge · amber = uncertain · red =
provably wrong · grey dashed = absent. SOLL: green = certain · amber = probable.

## Architecture (SOLL)

```mermaid
flowchart TD
  upstream["upstream — OSM · terrain · imagery · weather · sky"]
  providers["PROVIDERS · src/data"]
  store[("CONTENT STORE — hash = filename")]
  field["GROUND — height · slope · class · edges · water"]
  gen["GENERATORS — one part + capability, from (kind, params, seed, budget)"]
  comp["COMPOSITORS — one draw list: places · culls · quantises · batches"]
  rend["RENDERER — pixels from a declared plan"]
  frame(["720p60 on this device"])
  scen[/"SCENARIOS — camera × clock × world-or-studio"/]

  upstream --> providers --> store --> field --> gen
  gen -->|part| store -->|handle| comp -->|draw list| rend --> frame
  scen -.->|declares| gen & comp & rend
```

| layer may not spell | |
|---|---|
| Ground | camera · frustum · clock · LOD level · device · sun |
| generator | camera · neighbour part · draw list · device |
| compositor | device · pipeline · texture · shader · pass |
| renderer | any content noun |

Peers never call each other; a part-on-part dependency travels as data through Ground.
Budget = screen-space error in px, quantised to a global ladder before it becomes a key;
key = `(kind, params, seed, rung)` value, no strings. Degrade on detail, refuse on existence.

### The actor chain (SOLL — the owner's causal decomposition, general)

```mermaid
flowchart TD
  B["BODY — geometry, glTF parts: vehicle · walker · aircraft · door · pump"]
  B --> A["ACTUATORS — the functions a body declares: steer · drive · brake · lamps · walk · open"]
  A --> P["PHYSICS — forces at the contacts; only integration places a body"]
  C["CONTROLLER — a mind or the player POSSESSES the seam"] -->|acts on| A
  C -->|perceives| Q["PERCEPTION — bounded spatial queries: bounds · ground · sight"]
  C -->|asks| N["PATHFINDING — two coordinates in, corridor out: walk · drive · fly · rail"]
```

The chain holds for EVERYTHING that moves, with or without a mind: a parked car is a body whose
seam nobody possesses; a door is a body with one actuator; an NPC differs from the player only
in who possesses the seam (Unreal's Pawn/Controller possession — the reference). Perception is
spatial query over bounds, never privileged state; navigation is one pathfinding service with
modes, handed as a TOOL ; physics binds to actuators, never to the controller.
`Journey` folds into `Sim` along this chain. **A client includes nothing but
`include/outshine/`** — enforced by the build.

### The component model (SOLL — the decided reference design)

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

## Render plan (IST — what executes, judged as architecture)

```mermaid
flowchart TD
  subgraph compute
    T["mediumTransmittance"] --> M["mediumMultiScatter"] --> R["mediumRadiance"]
  end
  subgraph raster
    LV["lightVisibility"] --> SUBJ
    SKY["sky"] --> HDR[("SceneHdr")]
    SUBJ["subjects"] --> HDR
    GLASS["subjectsTransmissive — a cloned stage"] --> CT["compositeTransmission"]
    HDR --> TAA["temporalResolve — encodes nothing, folded into tonemap"] --> TONE["tonemap"]
    TONE --> OV["overlay"] --> P["present"]
  end

  classDef sound fill:#1f6f3f,stroke:#0d3b21,color:#fff
  classDef unsure fill:#8a6d1f,stroke:#4a3a0d,color:#fff
  classDef wrong fill:#7a2222,stroke:#3d1111,color:#fff
  class T,M,R,SKY,LV,CT,TONE,OV,P sound
  class TAA unsure
  class SUBJ,GLASS wrong
```

Green = sound abstraction by current knowledge; amber = form in question; red = provably wrong
(subjects: six responsibilities, instancing a literal, nothing culls; glass: a full clone of the
subject stage). One `Writes` producer per derived resource (`static_assert`); missing
contributor = picture choice, **published** as `-> neutral`; load/store ops derived from the
plan (`Stored()`).

## Render plan (SOLL — the declared target)

```mermaid
flowchart TD
  subgraph compute
    T2["mediumTransmittance"] --> M2["mediumMultiScatter"] --> R2["mediumRadiance"]
    R2 --> IR["irradiance"] --> AE["autoExposure"]
  end
  subgraph raster
    LV2["lightVisibility"] --> GEO
    SKY2["sky"] --> HDR2[("SceneHdr")]
    SUN["sun"] & MOON["moon"] & STARS["stars"] --> HDR2
    GEO["terrain · buildings · water · models"] --> HDR2
    SUBJ2["subjects — resident, culled, instanced"] --> HDR2
    GLASS2["transmissive draws in the one subject stage"] --> CT2["compositeTransmission"]
    AO["ambientOcclusion"] --> HDR2
    HDR2 --> TAA2["temporalResolve"] --> TONE2["tonemap"] --> OV2["overlay"] --> P2["present"]
  end

  classDef sure fill:#1f6f3f,stroke:#0d3b21,color:#fff
  classDef likely fill:#8a6d1f,stroke:#4a3a0d,color:#fff
  class T2,M2,R2,LV2,SKY2,SUBJ2,CT2,TONE2,OV2,P2,GEO,SUN,MOON,STARS,IR sure
  class AE,AO,TAA2,GLASS2 likely
```

Green = certain; amber = probable (auto-exposure shape, AO method, TAA's place, and whether
transmissive draws fold into the subject stage are open design calls).

## Class structure (IST)

```mermaid
flowchart TD
  Transport --> WebTileSource --> ContentStore --> TerrariumDem & VersatilesVector
  TerrariumDem --> GroundStream
  TilePool --> GroundStream & OsmField
  VersatilesVector --> OsmField --> RoadHarvest --> Wayfinding
  OsmField --> StreetField & BuildingField & WaterField
  GroundStream --> Ground --> Forest & Buildings & Water & Infrastructure
  Wayfinding --> ReferenceLine --> Carriageway --> Ribbon
  Carriageway --> SpeedProfile --> Pilot --> Walk & Drive & Fly & Rail
  Drive --> Rig --> Body
  Rig --> Contact & Shear
  Forest & Buildings & Water & Ribbon & Subject --> DrawList
  DrawList --> SubjectDraw --> Renderer
  MediumTransmittanceStage --> MediumMultiScatterStage --> MediumRadianceStage --> SkyStage --> Renderer
  LightVisibilityStage --> Renderer --> TonemapStage --> PresentStage
  Live --> Renderer
  Ephemeris & RegionForge --> Sim --> Renderer
  Frustum -.-> DrawList
  TilePool --> World["World — quadtree LOD · admission · kerbs"] --> Sim
  GroundStream --> Journey["Journey — the drive, living in tools/"]
  Journey -.-> Sim
  Engine --> Live
  GltfStudio --> Renderer
  Assembly["Assembly — the XML door"] --> SceneStore["Scene Store — entities · typed pairs · traits · tags"]

  classDef sound fill:#1f6f3f,stroke:#0d3b21,color:#fff
  classDef unsure fill:#8a6d1f,stroke:#4a3a0d,color:#fff
  classDef wrong fill:#7a2222,stroke:#3d1111,color:#fff
  class Transport,WebTileSource,ContentStore,TerrariumDem,VersatilesVector,GroundStream,OsmField,RoadHarvest,Wayfinding,StreetField,Ground,Forest,Buildings,Water,Infrastructure,ReferenceLine,Carriageway,Ribbon,SpeedProfile,Pilot,Walk,Drive,Fly,Rail,Rig,Body,Contact,Shear,MediumTransmittanceStage,MediumMultiScatterStage,MediumRadianceStage,SkyStage,PresentStage,Engine,SceneStore,Assembly sound
  class BuildingField,WaterField,Subject,DrawList,Renderer,TonemapStage,LightVisibilityStage,Frustum,Ephemeris,RegionForge,GltfStudio unsure
  class TilePool,World,SubjectDraw,Sim,Live,Journey wrong
```

Colours are ARCHITECTURE, adjudicated by an independent review (2026-08-22): green = right
responsibility in the right layer; amber = form in question (fields that tessellate, the getter
carpet, TAA folded into tonemap, idle values); red = provably wrong — `TilePool` and `World`
spell camera and LOD inside the ground layer, `SubjectDraw` is six responsibilities, `Sim` and
`Live` are hand-wired god facades the component model replaces, `Journey` is engine work in a
tool. The rot concentrates at the orchestration edges; the middle of the tree is sound.

## Class structure (SOLL — where the tree is going)

```mermaid
flowchart TD
  Scenario["Scenario XML"] --> Assembly
  ClientCode["client C++"] --> Assembly
  Assembly --> SceneStore["Scene Store — entities · pairs · traits · tags · slots"]
  SceneStore --> Columns["Columns — vehicle numbers · placements, by handle"]
  SceneStore --> SimD["Sim — owns the drive: corridor · speed plan · pilot, Journey folded"]
  SimD --> Pathfinding["Pathfinding tool — walk · drive · fly · rail"]
  SimD --> Physics["Rig · Body · Contact — forces at the patch"]
  SimD --> Compositors["Compositors — terrain · ring · cut-fill placement, leaving the stills driver"]
  Compositors --> DrawList --> Registry["stage registry — the executor table"]
  Registry --> Stages["stages: source · residency · encode split"]
  Stages --> Frame(["720p60"])
  Entities["entity store + culling"] -.-> DrawList

  classDef sure fill:#1f6f3f,stroke:#0d3b21,color:#fff
  classDef likely fill:#8a6d1f,stroke:#4a3a0d,color:#fff
  class Scenario,ClientCode,Assembly,SceneStore,SimD,Pathfinding,Physics,Registry,DrawList,Frame sure
  class Columns,Compositors,Stages,Entities likely
```

## Public interface (IST)

```mermaid
classDiagram
  direction TB
  class Engine {
    +Read(path) bool
    +Load(path) bool
    +Declare(scenario) bool
    +Declared() Scenario
    +Carried() strings
    +Advance() bool
    +Run() bool
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
    +RenderFrame() / ReadPixels()
    +WhyNot() string
  }
  class GroundStream {
    +At(lat, lon) GroundSample
    +BlockAt(z, x, y) GroundBlock
    +PostM(latDeg) double
  }
  class Journey {
    +Lay(between, scenario, zoom, wire, sink) bool
    +Ride(dtS, taken) Ridden
    +Corridor() ReferenceLine
    +Ground() GroundStream
    +Carried() Body
  }
  Engine --> Live : owns
  Live --> Renderer : drives
  Journey --> GroundStream : owns
  Sim --> GroundStream : owns
```

`Journey` still stands outside the library (it folds into `Sim`); `Live`, `Renderer`,
`GroundStream` and `Journey` are still reachable by clients — the SOLL below removes them.

## Public interface (SOLL — one door)

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

`test/run.sh` is the only runner (one process per test, real verdict, includes per layer = the
build's own sets). `tools/` builds ON the library, runs only by name. Oracle pipeline:
fetch → generate → patch (both sides) → convert → Cycles → compare (perceptual tail / geometric
bound, 0.005 px floor); **criteria met** and **cases within bound** published side by side.
Read the trailer first — a count without `N tests: … PASS … FAIL` may measure the past.
The one offline script: `test/harness/shared/corpus/prepare.py`.

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
(task→feature only) · `Area` (the tree's layers) · `Tags` · `Depends` · `Regresses` · `Supersedes`.
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
| `tools/` | `tools/driver/` (the test-drive game: worldwide A→B, FPV-first, declarative), `tools/viewer/`, `tools/host/` |
| `Makefile` | build · test · clean, nothing else |
| `board/` | the working system (above) |

References (fetched, not recalled): C++ Core Guidelines (binding) · Gregory GEA3 · RTR4 · PBR4 ·
Frostbite PBR/FrameGraph · Nanite (error-driven LOD) · Decima (runtime placement) · id Tech 7
(hard frame floor) · RAGE (decisionless pools) · SpeedTree (LOD cross-fade). Distance-ratio LOD
selection is refused by construction — one currency: projected error.
