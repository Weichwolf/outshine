# Outshine

**A modern game engine combining the best of RAGE and Unreal.** Development platform IS the target:
Apple A18 Pro (2P+4E cores, 5 GPU cores, 8 GB, Metal 4), **720p60 held** — p50/p95/p99 over a moving
camera, never a mean.

**THE BENCHMARK IS A QUARRY, NOT A SPECIFICATION.** Where RAGE or Unreal settled a question their
answer is evidence and departing carries the burden — but the decision is mine and the REASON is
what I owe. Unreal can be read; RAGE is reconstruction, so it carries less, and neither outranks
a measurement of THIS tree.

**What an engine IS**: an interactive physics simulation with a focus on graphics — physically as
accurate as NECESSARY, graphically as good as the FRAME BUDGET allows, temporally DETERMINISTIC.
Each of those three is a bound, and the middle one is the bound "as good as possible" does not
carry: a target without a ceiling cannot be missed, and a stage that was as good as possible six
times over is the god stage. **The engine knows no cars and no aircraft** — only bodies, forces,
actuators and control. A control command comes from the player through bindings or from a mind;
it ACTIVATES a force, and only integration places a body. RAGE keeps `CVehicle` in the game layer
and Unreal keeps wheeled movement in a plugin; a vehicle noun inside the engine core is a finding
wherever it stands.

- **THE ENGINE READS ALL OF glTF 2.0 AND ACTS ON ALL OF IT.** Not the subset a scene happens to
  need -- the format entire, core and every ratified extension, and a feature that is parsed but
  changes no pixel is not read. This is the one capability in the tree a VENDOR can certify:
  Khronos ships the sample assets and the validator, so the distance to "all of it" is a number
  someone else computes. **And it sets the bar for the in-process form**: whatever the reader takes
  from a FILE, `include/Geometry.h` must be able to carry, or a client's generator is strictly
  weaker than a file and the interchange claim is false
- **SDL3 is REQUIRED and `include/Outshine.h` says so** by including it: the CLIENT owns the process and calls `SDL_Init`, the library never does. **SDL_GPU is one renderer, not the door** — `SDL_Window` is SDL3's core, so the door stays renderer-neutral. **glTF 2.0** is the only content surface
- **C++23**, `-Wall -Werror -Wpedantic`, one `-std` for the whole tree; `static_assert` and the type system over checkers; `std::span`/`std::string_view` at boundaries, `std::mdspan` for field and instance views, `std::expected` where a refusal carries its reason
- **Precision has ONE boundary and it is the camera**: the scene keeps 64-bit positions and the
  renderer is camera-relative in 32-bit — `Anchor - Eye` in `double`, the model-view-projection
  product in `double`, and the cast to `float` only at the uniform push
  (`src/render/stages/SubjectDraw.cpp:731,733,736`). A `float` that ever holds a world position
  is a defect; a `double` that reaches a shader is a different one
- **Private is the DEFAULT and a wider door justifies itself** in the item that widened it. What
  is private can be changed; a public data member is an invariant nobody can hold. Composition is
  the usual answer, inheritance the right one where a stable interface carries shared machinery.
  `--audit-access` counts what stands wider and refuses when the count moves
- **SIMD- and optimization-friendly**: contiguous, one-width, pointer-free layouts; fast path on the hot path; batch over per-item; bounded terms on the frame path (no alloc/lock/disk/unbounded block)
- **THE SIMULATION IS MECHANICS AND THE RENDERER IS OPTICS.** Both are branches of physics, and
  naming them so decides what may appear in either: the simulation speaks of bodies, joints,
  constraints, wrenches, impulses and integration; the renderer speaks of radiance, irradiance,
  transmittance, scattering, extinction, albedo and exposure. Neither speaks of cars.
  **Measured, the renderer already obeys and the simulation does not**: `src/render` carries 390
  optics terms against 3 subject words, while `src/sim` carries 46 mechanics terms against 96
  vehicle words -- two to one the wrong way. The reason is plain once seen: optics never met a
  subject, because light knows no cars, and the simulation was written from one car outwards
- **AN ENGINE KNOWS LAWS AND NO SUBJECTS.** Its vocabulary is the laws': **body · joint · degree
  of freedom · drive · constraint · force · contact · integration**. A vehicle, wheel, tyre, seat,
  door or walker is a SUBJECT — an assembly a scenario builds from that vocabulary — and the
  engine never names one. RAGE keeps `phBound`/`phConstraint` in physics and `CVehicle`/`CWheel`
  in the game layer; Unreal keeps `FBodyInstance`/`FConstraintInstance` in the engine and wheeled
  movement in a plugin outside it.
  **There is no actuator in physics** — only a constraint with a target and a force limit, which
  is what Chaos and PhysX call a joint DRIVE. Motor and brake are ONE drive on one degree of
  freedom, separated by whether it may add energy; a LEVER is a ratio in the same statement.
  **`walk` and `open` are CONTROL over time**, a controller commanding drives, never actuation.
  A convenience outshine ships — a raycast-and-spring wheel — ships as a DECLARED ASSEMBLY in the
  catalogue and never as an engine type, which is where both benchmarks put theirs
- **THE INTERFACE IS A DOCUMENT, and the scenario declares which one.** Markup, style, layout,
  type and pointer are engine verbs; the panel itself is CONTENT and lives beside the glTF, never
  in C++. RAGE reaches the same answer through Scaleform and Unreal does not reach it at all --
  a UMG tree is authored in an editor, which makes it a build artefact rather than declared data.
  What this choice buys beyond the shape: **the format has a standards body and the standards body
  ships the corpus**, so WPT and test262 certify this layer from outside, and nothing else in the
  tree has that
- **THE DOOR'S IMPLEMENTATION IS A SKELETON OF SUBSYSTEMS**, never a struct of members: each
  concern OWNS its state behind a small door, and the engine states the ORDER they run in. A
  defect then has an address, and the phase order is read without reading a body
- **Declarative**: scenarios declare, the engine behaves; content = data, engine = verbs; the consumer selects from a `constexpr` catalogue and cannot add to it. **A section that is NOT declared decides nothing** — its `Declared` flag is read where the decision is made, and what stands in its place is the engine's own default, never the zeroes of a struct nobody filled in
- **A SCENARIO IS A STREAM, not a value that is re-declared.** `Declare` seeds; after that parts
  enter and leave. The bound is a cost: the work a declaration causes is proportional to what it
  CHANGED, never to how big it is. Neither benchmark rebuilds a world to change part of one
- **A WORLD TEMPLATE IS A FILE, NOT A VERB.** Earth and moon are scenario XML shipped under
  `src/assets`, and a scenario reaches one the way it reaches any other -- `Layer{Id, Path, Set}`,
  already in the declaration -- then states its deltas over it. There is no `Planet(params)` in the
  engine and there must not be: a factory that returns a Scenario value is engine code doing what a
  data file does, and it puts a world behind a compiler instead of in front of an editor. Unreal
  ships template MAPS and RAGE ships map files; neither ships a world constructor
- **Batteries as declarations**: outshine ships convenience components -- generators and providers
  -- as catalogue citizens the scenario selects; the engine core stays scenario-agnostic
- **Every number carries its origin** (derived · measured · `[SET]`) with unit and population; no magic numbers; calibration measures, never decides
- **The code carries NO comments** — `src/`, `include/` and `apps/` hold no `//`, no block, no
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
- **GREP BEFORE YOU WRITE.** A new type, function or file is preceded by a search for what it
  would do; a capability that looks absent is usually present and unreachable, and that is the
  finding
- **Cycles is the oracle** for correctness; references are for ambition; the corpus is a driver, not a certificate
- **ONE WORLD, and everything else is a VIEW of it.** One space is a convention; one WORLD is a
  holder, and the second holder is what makes two subsystems disagree about the same place.
  Unreal has one `UWorld` and the renderer takes a SCENE PROXY of it; RAGE streams one map and
  `fwEntity` lives in that one. A drive, a picture and a generator read the same world through
  bounded views, never through worlds of their own
- **ONE PRE-VIEW TRANSLATION PER FRAME, and every matrix reads it.** Precision has one boundary
  and it is the camera, and this is the same sentence from the matrix side: the frame chooses one
  origin, and the view, the light and every instance transform are built against THAT one.
  Unreal names it `FViewMatrices::PreViewTranslation`. A subsystem that subtracts an origin for
  itself is the defect whoever gets it right, because the next one will get it wrong
- **The world STREAMS by cell, with its content.** A cell brings its ground, its structures and
  its actors in and out together; nothing holds the whole of anything. Unreal's World Partition
  and RAGE's map nodes are the same answer, and it is the answer both engines are built around
  rather than one they added
- **A failure is loud; something is always drawn; delete on the day you replace**
- Artefacts go to the system temp dir, never the tree; `git log` is what was — no journal

**Diagram colours** — CURRENT: green = correct by current knowledge · amber = uncertain · red =
provably wrong · grey dashed = absent. TARGET: green = certain · amber = probable.

## Who reads this

**This file is the DEVELOPER's, and it is VISION and TARGET — never a description.** I read it on
every turn and drive with it: pick the next item, repair it, close it with a proving test and a
negative control. **TWO reviewers read from OUTSIDE the change**, each against its own brief in
`.claude/agents/`, each filing findings and neither editing `src/`. I advance, they CORRECT.

| who | judges | never |
|---|---|---|
| **architect** | structure: layering, abstraction, the door, what a scenario can REACH | takes a screenshot, signs off the picture |
| **stakeholder** | the PICTURE: `apps/driver` as a test drive at Gran Turismo 7's level, on short routes it picks itself | judges architecture |

The asymmetry is not tempo but standing: inside the work I can be wrong and measure my way out
before the hour is over. A finding either of them files becomes work, so it takes nothing on my
word — each runs the gate itself and reads the tree, not my account of it. **`STATE.md` is
CURRENT for all three of us**, generated by every `make` and written by no hand.

## CURRENT is `STATE.md` and TARGET is here

**This file carries only what the tree is going TOWARD. What it IS lives in `STATE.md`, which
every `make` regenerates and no hand writes** — the door's verbs, the module graph with any cycle
named, the tier table, the heaviest units, the widest surfaces, colliding header names, the
sources no suite links, what stands wider than private, what is declared red, which suites reach
past the door, every named constant standing as a bare literal, and the PROGRESS table.

**After every gate run, read `STATE.md` and name what still departs from RAGE and Unreal.** The
numbers there are the distance; the board is where the answer goes.

**PROGRESS measures nine areas against RAGE and Unreal, and it is counted from `board/`** —
where the target already lives, so there is no second list to keep. An item declares
`Progress: <area>` and its checkboxes are that area's predicates; a ticked one must NAME ITS
PROOF, a case or an audit flag, and a tick whose proof this tree does not hold is REPORTED rather
than counted. A predicate states a behaviour or a reachability, never a name: counting declared
class names would have scored the world generators complete while 6528 lines of them sat in the
archive with no path from any declaration. The denominator is visible and it GROWS, because
discovering work is not progress.

**A diagram here is an intention and never a description.** When the two disagree the tree is
what `STATE.md` says, and the distance between them is the work.

## What the benchmark already settled

**For every question here one of the two already has the correct answer, and TARGET's job is to
hold the better of the two — not to invent a third.** Where they agree the answer is not in doubt.
Where they differ the row says which one wins and WHY, and that reason is the part I owe. Unreal
can be read and RAGE is reconstruction, so a RAGE row carries less and says so.

| question | Unreal | RAGE | TARGET takes | why |
|---|---|---|---|---|
| **module boundary** | `Build.cs` names public deps; `Public/`+`Private/`; `*_API` exports | fw/rage libraries, `CGame` above them | **Unreal** | the COMPILER enforces it through include paths — a layering audited after the fact is a convention, and a convention is how a 44-header drawer forms |
| **scene** | `UWorld` → `FScene` + `FPrimitiveSceneProxy`, fed by DELTAS | `fwEntity` on scene-update lists | **Unreal** | game state and render state are separate objects joined only by an explicit delta; that IS "one world, the rest are views", and it is what lets the renderer keep GPU-side state across frames |
| **frame path** | `FGPUScene` instances GPU-side, Nanite culls in compute, ONE indirect draw | per-batch draw calls | **Unreal** | no CPU term scales with geometry or lights; this is the half of the render plan a stage graph does not carry |
| **streaming** | World Partition: cell grid, data layers, **HLOD** | map nodes, IMAP/ITYP, LOD hierarchy | **both agree** | a non-resident cell is represented COARSER, never by nothing — the horizon is the proof and both engines pay for it |
| **resources** | import → DDC → cooked platform data | **map the bytes, fix the pointers, no parse** | **RAGE** | a load that parses cannot keep up with a camera; zero-parse is what makes cell streaming affordable, and the content store is already hash-addressed for it |
| **behaviour** | Behavior Tree, blackboard | **`CTask` tree**: a task owns sub-tasks, yields, and is abandoned as a subtree | **RAGE** | for a PHYSICAL actor -- a driver, a walker -- hierarchical tasks decompose the way the act does; a behaviour tree re-decides from the root every tick |
| **possession** | `AController` possesses a `APawn` | ped/vehicle relationship | **Unreal** | the seam between a mind and a body is the same seam a player plugs into, and that is why one interface serves both |
| **time** | variable step, physics substeps | fixed step, replay- and network-exact | **RAGE** | "temporally DETERMINISTIC" is not a wish: a fixed simulation step, one fixed order, and INTERPOLATION to the display is the mechanism, and nothing else delivers it |
| **threading** | `FTaskGraph`, render + RHI threads, workers | `sysTaskManager`, fibers | **both agree** | a task graph with explicit dependencies; 720p60 on four usable cores is not reachable from one thread, and retrofitting threading is a rewrite |
| **interface** | Slate/UMG: a native widget tree authored in the editor; CEF only for real browsers | **Scaleform GFx**: Flash documents with ActionScript, authored OUTSIDE and rendered by the engine | **RAGE's slot, with a standardised format in it** | a widget tree authored in an editor is not declared CONTENT, and this engine's rule is content = data. HTML/CSS/JS fills Scaleform's slot with a format a standards body maintains -- and that buys the thing neither Scaleform nor Slate has: **WPT and test262 prove the interface layer, so it is the one subsystem in this tree whose correctness someone else certifies** |
| **engine composition** | thin `UEngine`; each concern a SUBSYSTEM with a declared lifetime | **`gameSkeleton`**: INIT/UPDATE/SHUTDOWN steps in declared PHASES | **both** | they answer different halves — who OWNS the state, and WHEN it runs. A flat struct of members has neither: every concern reaches every other, and the order is whatever the bodies happen to do |
| **content surface** | `.uasset` + Interchange for import | offline tool chain | **neither wholesale** | glTF 2.0 is the only content surface here, which is Interchange's role without Interchange's format; what a client hands ACROSS the door is a handle, never a layout |

## Architecture (TARGET)

```mermaid
flowchart TD
  upstream["upstream — OSM · terrain · imagery · weather · sky"]
  providers["PROVIDERS · src/data"]
  store[("CONTENT STORE — hash = filename")]
  field["GROUND — the declared sphere's surface fields: height · slope · class · edges · water; one stack PER sphere, empty fields allowed, absent in free flight"]
  gen["GENERATORS — A LIBRARY OF ITS OWN: one part + capability, from (kind, params, seed, budget); registry of what ships PLUS what a client registered; links without the engine"]
  comp["COMPOSITORS — one draw list: places · culls · quantises · batches"]
  rend["RENDERER — pixels from a declared plan"]
  frame(["720p60 on this device"])
  scen[/"SCENARIOS — camera × clock × world-or-studio"/]
  ui["INTERFACE — documents: markup · style · layout · type · pointer, script in base; RAGE fills this slot with Scaleform, and a standards body ships this one's corpus"]

  actors["ACTOR CHAIN — bodies · minds · presence, assembled from the scene store"]

  upstream --> providers --> store --> field --> gen
  gen -->|part| store -->|handle| comp -->|draw list| rend --> frame
  field --> actors -->|placements| comp
  scen -.->|declares| gen & comp & rend & ui
  ui -->|overlay| rend
  scen -.->|declares · clocks| actors
  tmpl[/"WORLD TEMPLATES — earth · moon: scenario XML in src/assets, NOT engine"/] -.->|layered under| scen
```
```mermaid
flowchart TD
  B["BODY — geometry, glTF parts: vehicle · walker · aircraft · door · pump"]
  B --> J["JOINTS — the degrees of freedom between two bodies: revolute · prismatic · fixed"]
  J --> A["ACTUATORS — one per degree of freedom: EFFORT (force/torque, dissipative or not) or MOTION (position/velocity), through a LEVER (ratio)"]
  A --> P["PHYSICS — forces at the contacts; only integration places a body"]
  C["CONTROLLER — a mind or the player POSSESSES the seam; walking and opening are CONTROL over time, never actuators"] -->|acts on| A
  C -->|perceives| Q["PERCEPTION — bounded spatial queries: bounds · ground · sight"]
  C -->|asks| N["PATHFINDING — two coordinates in, corridor out: walk · drive · fly · rail"]
  PR["PRESENCE — field → rails → body; a MEASUREMENT materialises"] -.-> B
```
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

## Render plan (TARGET)

**THE PLAN IS A GRAPH AND THE FRAME PATH IS GPU-DRIVEN.** The stage graph below is Unreal's RDG
shape and it is half the answer; the other half is what the CPU spends per pass. Instances live in
GPU buffers, culling runs in compute, and a pass is ONE indirect draw rather than one call per
batch (`FGPUScene`). Lights are assigned to clusters by a compute stage and the shading pass reads
the grid, so a light budget is a GPU allocation and never a constant in a header. The shadow atlas
carries cascades and the shader picks one. **No CPU term on this path scales with geometry, lights
or pixels.**

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
  Physics --> Underfoot["UNDERFOOT — what a wheel stands on: height · normal · friction, from the world and never from the corridor"]
  Underfoot --> WorldC
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
  class Underfoot sure
```

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
  class Geometry {
    +Part(named, material) int
    +Positions, Normals, Texture, Tangents, Colours, Triangles
    +the reader fills one, a generator fills one, a client fills one
    -storage private: the layout stays free to move
  }
  Engine --> Scenario : reads
  Engine --> Geometry : Stands(one)
  Engine --> Store : owns the one graph
  Geometry --> Store : what a subject is made of
```

## Board

```mermaid
stateDiagram-v2
  [*] --> open : filed (defect found = item, same round)
  open --> active : being worked NOW (groom parent/depends)
  active --> [*] : the file is DELETED; the commit names the proving test
  active --> open : parked
  open --> withdrawn : the PREMISE was wrong -- the defect is not there
  active --> withdrawn : the same, found while working it
  withdrawn --> [*] : the file is DELETED; the commit says what was misread
```

**An item leaves in two ways and they are not the same.** A CLOSURE says a defect was real and is
gone, and it names the proving test. A WITHDRAWAL says the defect was never there and names what
was misread instead -- a measure that was absent rather than zero, a number read from the wrong
frame, a capability that turned out to be reachable. Deleting a withdrawn item as though it had
been closed loses the more useful half: what the filer believed, and why the tree looked that way.
Both states are recorded before the file goes, so `AnItemReachesClosedThroughActive` can tell them
apart.

`board/` is ONE FLAT DIRECTORY. One file = RFC 822 header + markdown body. Fields: `Type`
(feature|task|bug|issue) · `State` (open|active) · `Parent` · `Area` · `Tags` · `Depends` ·
`Regresses` · `Supersedes`. Filename `NNNN_label.md`; number = identity; no dates — git is the
truth. **Closing is DELETING the file**: what it said is in the commit that removed it.
Titles say what WILL BE TRUE. Commits reference `board:NNNN`. `State: active` marks what is
being worked on right now — always.

**A REFACTOR TO TARGET BLOCKS THE BOARD.** While one stands `active`, nothing outside it is
worked — not a feature, not a gap, not a finding either reviewer files. Those are recorded and
they wait. An item repaired on the architecture that is about to be replaced is work done twice,
and the second time is the one that counts. The order does not vary: **ask whether TARGET matches
the best of RAGE and Unreal and repair TARGET first if it does not · rebuild onto TARGET · then
close the feature gaps.** A refactor toward a target that is short of the benchmark spends the
effort and arrives somewhere that still has to be left.

**GIT IS THE LOGBOOK. The item is what is true NOW.** A newer measurement REPLACES the older one
it corrects; a paragraph the tree has overtaken is deleted, not appended after. A closure states
what became true and names the proving test and its negative control — the derivation, the
numbers and the story belong in the commit message, which is where anyone looks for what
happened. An item that has grown three stacked rounds of prose needs rewriting, not a fourth.

```sh
grep -l '^State: active' board/*.md                  # in flight NOW
grep -l '^Type: bug' board/*.md                      # by kind
grep -l '^Parent: 0007' board/*.md                   # a feature's children
git log --grep 'board:0042'                          # every commit on an item, and its closure
ls board/*.md | grep -o '[0-9]\{4\}' | sort -n | tail -1  # next id, derived
```

## Setup

| | |
|---|---|
| `src/` | the library entire; `src/assets/` its declared data; no entry point, no test. **The directory IS the dependency tier and the tier is DECLARED**: `LayerReaches` in `test/run.sh` states what each may include and `--audit-layers` refuses a source that crosses it -- and refuses a CYCLE between two modules
inside one tier, which the tier table alone cannot see. `base/` (math · geo · format · spatial · io) reaches nothing; `content/` (gltf · shade) and `actor/` reach base; `world/` (ground · generators · data · sky · weather) reaches base and content; `render/`, `scene/`, `scenario/`, `ui/`, `audio/`, `host/`, `compositor/` and `sim/` reach what their row says; `engine/` reaches all of it and is the door's own implementation, which is why it is not called `clients/` — the clients live in `apps/`. Unreal declares the same thing per module in `Build.cs`; a layering that is only a convention is how a 44-header drawer forms (board:1902) |
| `test/` | **The vendor's word and ours stand apart and the directory says which.** `khronos/` · `wpt/` · `test262/` · `geographiclib/` are the corpora (`khronos/validator/`: 263 cases judged as a REFUSAL against Khronos's report); `harness/` their scorers and the board claims; **`outshine/` is OURS**. Everything under `test/` reaches the library through `include/` and NOTHING of `src/` |
| `apps/` | the CLIENTS, built ON the library and each a product. **A client is almost no code, and its LINE COUNT is a measurement of the door**: when a client needs much code, the interface is too complicated and the door is the finding, never the client. At HEAD `apps/driver` is 236 lines and `apps/viewer` 338, and both are too long and the driver is GROWING (board:1898): **`apps/driver`** is outshine's one integration test and the stakeholder signs it off; **`apps/viewer`** shows any scenario and becomes a scenario itself, layered over the one it shows (board:1880) |
| `Makefile` | build · test · clean, nothing else. `make` writes TWO archives: `liboutshine.a`, and `libgenerators.a` — the generator tier alone, its member list DERIVED from the closure the linker itself computes, never a second list kept by hand |
| `board/` | the working system (above) |

`make` builds the library and every program under `apps/` into `build/`. **`test/gate.sh` is the FAST GATE
for iteration** — about a minute, naming a subset of `run.sh` plus `make` and one drive; it prints
what it does NOT cover, because a gate that hides its bounds reads as coverage. `test/run.sh` is the
only TEST runner and runs nothing else; by default it runs the corpora and the claims, while
`tools` and `apps` run when named. A standing RED is declared in `EXPECT_FAIL` with its count,
and the gate turns red the day such a case passes with the declaration still in place.

**THE GENERATORS ARE A LIBRARY WITH THEIR OWN DOOR.** outshine ships a registry — forest,
buildings, water, infrastructure — a client REGISTERS its own beside them, and another project
takes the tier alone with none of outshine's program behind it. A generator does not serialise:
it hands back the INTERNAL REPRESENTATION, and a glTF serialiser ships beside it for a caller who
wants a file. That representation is the UNIVERSAL interface for exchanging 3D data with
outshine — a glTF reader fills it, a generator fills it, a foreign program fills it with no file
anywhere, the compositor consumes it. One value, many producers, many consumers; **glTF is one of
its file forms and not its identity**. The shipped catalogue stays closed against a typo; a
client's generator enters as a VALUE with a handle, never a string.

**The front door is four headers**: `include/Outshine.h` the verbs, `include/Scenario.h` the
declaration, `include/Event.h` the return channel, `include/Geometry.h` the 3D value anything
hands in and outshine hands back — `std::span` and `std::string_view` and no outshine type, so a
foreign producer needs nothing of ours to fill it — `Host`, `Argument`, `Measure`, the shape RAGE
keeps in `fwEvent` and Unreal in its delegate header, apart from the engine door in both. outshine
loads a scenario and runs it — that is the whole of it. A client that needs an assembly view, a
transport or a parser is a client reaching past the door, and the door is what wants widening,
never the reach.

## The order the work is done in

```mermaid
flowchart LR
  A["1 · REFACTOR to TARGET"] --> B["2 · GUARDS: static_assert, the type system, refusal at assembly"]
  B --> C["3 · CORPUS CASES: a scenario against an invariant oracle"]
  C --> D["4 · JUDGE THE DRIVER: the stakeholder signs the picture off"]
  D --> E["5 · EXTEND"]
  E --> A
```

**EVERY CASE IS A SCENARIO WITH AN INVARIANT ORACLE.** A case declares what the engine should
stand up, the engine runs it, and the answer is compared against a reference whose truth does not
depend on our design. Nothing else is a test here — a case that asserts the shape of our own
architecture specifies nothing while TARGET moves, and blocks the refactor it should serve.

What a corpus HOLDS decides what it can prove:

| grade | it holds | it proves |
|---|---|---|
| **SPEC** | a standards body states the answer | conformance |
| **TRUTH** | a measurement or computation carried to more digits than we hold | correctness |
| **SNAPSHOT** | another implementation, frozen | agreement, never correctness |
| **INPUT** | nothing is supplied | that we survive it |

**A FUZZ CASE IS INPUT GRADE AND DETERMINISTIC** — every mutant a pure function of (seed,
position, kind). A random fuzzer belongs outside a gate that must not go silent; its output is a
reduced case committed here, never a tick. Its two controls: the schedule must produce documents
the reader REFUSES and documents that still STAND.

**ONLY THE VENDOR CORPORA PROVE ANYTHING.** Khronos, WPT, test262 and GeographicLib are where a
standards body or a computation carried further than ours states the answer, and a case there fails
because the code is wrong. Everything under `outshine/` is a REGRESSION NET of unknown
grade: it holds the tree to what the tree already did, which is agreement with ourselves and not
correctness. **This bites hardest during a refactor.** A restructuring that leaves our own cases
green has proven that it preserved the previous behaviour -- and if that behaviour was wrong, the
green is the wrong answer preserved exactly. So a red in an `outshine/` case during a refactor is
INFORMATION and not automatically a defect, and it is never made green by editing the case.

**`test/<vendor>/` is the vendor's word; `test/outshine/` is OUR OWN ORACLE and carries less.**
A khronos, wpt, test262 or geographiclib case is a SPECIFICATION: it fails and the code is
wrong, full stop. An `outshine/` case is a law of nature we implemented ourselves — static
equilibrium, a closed form, a conservation — and it fails in TWO ways: the code is wrong, or the
oracle is. So an `outshine/` case must carry its DERIVATION in prose beside the number, because
the derivation is the part a reader can check and the number is not. A self-built oracle that
states only a constant proves nothing but that we agree with ourselves.

Effort has two halves and the second one bites: FETCH is a pinned URL and a hash; REACH is priced
by the two-header door. `test/CORPORA.md` is the survey — which established corpus asserts which
capability of TARGET, at which grade, and what it costs to reach.

**A client that compiles against `include/` and renders proves more than any suite.**
`apps/driver` is outshine's one integration test and its product; the stakeholder signs the
picture off on screenshots it took itself.

**Two reviews land on alternating hours at :17, each in its own worktree** — the architect on
even hours, the stakeholder on odd. **Each is started with its brief and NOTHING else** — no
ticket list, no delta, no "check this first". A review handed my account of the hour reviews my
account; each reads CLAUDE.md, the tree and `git log` itself, and that independence is the only
thing that makes their findings worth having.

The architect owns both maps and measures the distance CURRENT → TARGET; the stakeholder owns
the picture and the sign-off. Each writes the next round's work order in `board/`.
