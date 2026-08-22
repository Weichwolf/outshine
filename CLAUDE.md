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

### The actor chain (SOLL — the owner's causal decomposition)

```mermaid
flowchart LR
  G["GEOMETRY — glTF parts"] --> F["FUNCTIONS — steer · drive · brake · lamps"]
  F --> P["PHYSICS — contacts, forces at the patch"]
  M["INTELLIGENCE"] -->|acts on| F
  M -->|sees| W["world queries — ground · corridor · sight"]
  M -->|asks| N["NAVIGATION — two coordinates in, corridor out"]
  PLAYER["player bindings"] -->|same seam| F
```

Physics binds to functions, never to the mind; the mind and the player actuate ONE seam.
`Journey` folds into `Sim` along this chain (`board:1581`). **A client includes nothing but
`include/outshine/`** — enforced by the build (`board:1582`).

### The component model (SOLL — reference design, `board:1583`)

Reference: **Flecs** (relationships + traits + script parity), supplemented by GAS tag algebra,
Smart-Object slots, CARLA's vehicle grammar. Written here, never a dependency.

```mermaid
flowchart TD
  XML["scenario XML"] --> API["ONE assembly API — same calls, same refusal text"]
  CPP["client C++"] --> API
  API --> STORE["entity store — ids and typed pairs (relation, target), values not pointers"]
  PRE["prefab / IsA — 'glTF as four-wheel', variants, named slots"] --> STORE
  STORE --> CAN["CAN — capability tags, constexpr catalogue (typo = compile error)"]
  STORE --> MAY["MAY — traits ON the relation: Exclusive · OneOf · Acyclic · With → refused at ASSEMBLY; situational = tag set-algebra at runtime"]
  STORE --> ACT["INTERACTS — world objects advertise slots as data; Free → Claimed → Occupied → Free"]
```

The XML reader is a serialisation of the assembly API against ONE graph — no second
representation, no converter (the Unity-baking rot). XML declares what the API can and nothing of
its own: no conditions, no control flow — behaviour belongs to the mind and the assignment. The
render plan stays a `constexpr` catalogue (unspellable beats refused-at-load); this graph is the
SCENE domain: vehicles, minds, tools, assignments, world objects. Banned by the sources
themselves: stringly-typed capabilities · content that ships a program · god actors ·
ECS-for-everything · unreserved shared affordances.

## Render plan (IST/SOLL per stage)

```mermaid
flowchart LR
  subgraph compute
    T["mediumTransmittance"] --> M["mediumMultiScatter"] --> R["mediumRadiance"]
    R --> IR["irradiance"] --> AE["autoExposure"]
  end
  subgraph raster
    LV["lightVisibility"] --> GEO
    SKY["sky"] --> HDR[("SceneHdr")]
    SUN["sun"] & MOON["moon"] & STARS["stars"] -.-> HDR
    GEO["terrain · buildings · water · models"] -.-> HDR
    SUBJ["subjects"] --> HDR
    GLASS["subjectsTransmissive"] --> CT["compositeTransmission"]
    AO["ambientOcclusion"]
    HDR --> TAA["temporalResolve"] --> TONE["tonemap"] --> OV["overlay"] --> P["present"]
  end

  classDef built fill:#1f6f3f,stroke:#0d3b21,color:#fff
  classDef absent fill:#7a2222,stroke:#3d1111,color:#fff,stroke-dasharray:4 3
  class T,M,R,SKY,LV,SUBJ,GLASS,CT,TAA,TONE,OV,P built
  class IR,AE,SUN,MOON,STARS,GEO,AO absent
```

Green = `Renderer::Executable` returns true and a suite proves it; red dashed = catalogued, refused
loudly by name. One `Writes` producer per derived resource (`static_assert`); missing contributor =
picture choice, **published** as `-> neutral`; load/store ops derived from the plan (`Stored()`).

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
  Entities -.-> DrawList
  Assembly["Assembly — the XML door"] --> SceneStore["Scene Store — entities · typed pairs · traits · tags"]

  classDef built fill:#1f6f3f,stroke:#0d3b21,color:#fff
  classDef idle fill:#8a6d1f,stroke:#4a3a0d,color:#fff
  classDef absent fill:#7a2222,stroke:#3d1111,color:#fff,stroke-dasharray:4 3
  class Transport,WebTileSource,ContentStore,TerrariumDem,VersatilesVector,TilePool,GroundStream,OsmField,RoadHarvest,Wayfinding,StreetField,BuildingField,WaterField,Ground,Forest,Buildings,Water,Infrastructure,ReferenceLine,Carriageway,Ribbon,SpeedProfile,Pilot,Walk,Drive,Fly,Rail,Rig,Body,Contact,Shear,Subject,DrawList,SubjectDraw,Renderer,TonemapStage,PresentStage,Live,Sim,Assembly,SceneStore,Ephemeris,MediumTransmittanceStage,MediumMultiScatterStage,MediumRadianceStage,SkyStage,LightVisibilityStage built
  class Frustum,RegionForge idle
  class Entities absent
```

Green = in a passing suite's source list; amber = compiled, run by nothing; red dashed = target
needs it, not in the tree. Known departures (each a board item): instancing passes a literal 1 and
nothing culls (`board:1538`); glass is a cloned stage and content change = full rebuild
(`board:1574`); shading samples no atlas yet (`board:1575`); `SubjectDraw` is six responsibilities
(`board:1577`).

## Public interface (IST)

```mermaid
classDiagram
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
  Journey --> GroundStream : owns — folds into Sim, board:1581
  Sim --> GroundStream : owns via World
```

Headers must read like a good book: `include/outshine/Outshine.h` is the four-line client;
`src/corridor/Ribbon.h`, `src/world/TerrainLoader.h`, `src/render/plan/RenderCatalogue.h` are the
models to match.

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
| `tools/` | `tools/driver/` (the test-drive game: worldwide A→B, FPV-first, declarative — `board:1573`), `tools/viewer/`, `tools/host/` |
| `Makefile` | build · test · clean, nothing else |
| `board/` | the working system (above) |

References (fetched, not recalled): C++ Core Guidelines (binding) · Gregory GEA3 · RTR4 · PBR4 ·
Frostbite PBR/FrameGraph · Nanite (error-driven LOD) · Decima (runtime placement) · id Tech 7
(hard frame floor) · RAGE (decisionless pools) · SpeedTree (LOD cross-fade). Distance-ratio LOD
selection is refused by construction — one currency: projected error.
