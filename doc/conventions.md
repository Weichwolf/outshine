# Conventions

Language, names, structure, and what must never appear in the code.

## The working rule (spec first)

This documentation is **spec-driven**: every topic file carries the same four sections, and they have
different owners in time.

| Section | Content | Changes when |
|---|---|---|
| `## Spec` | the contract: what the thing must be able to do, acceptance criteria, measurement anchors | only by **decision** — never by building |
| `## State` | what is built, with commit and measurement. Honest, including "nothing" | when a round lands |
| `## Gaps` | the difference Spec − State, ordered by value, **including rejected approaches with their measurements** | when a round lands |
| `## Knowledge` | derivations, formulas, measured constants | when something is derived or measured |

A round that intends to change behaviour therefore runs like this:

1. **Change the Spec of its topic file first.** If the round cannot say what the contract becomes, it
   is not ready to start. A Spec change is a decision and is made as one.
2. **Build until State meets Spec** — measured against the Spec's own anchors, not against a feeling.
   Measurements beat inspection; the mission control loop ([`build-and-ops.md`](build-and-ops.md)) is
   how a claim about behaviour gets settled.
3. **Update State and Gaps, and add one line to [`journal.md`](journal.md)** (commit, what it built,
   what it measured).
4. **Rejected approaches stay in Gaps** — with their measurements. A measured failure is knowledge;
   deleting it means someone re-runs the experiment.

Two consequences worth stating: there is no second list of open work anywhere (no `TODO.md`, no
trailing "open points" per file — Gaps is the one place), and `CLAUDE.md` is touched only when a
session-start fact changed, kept under 100 lines.

## Language

**C++17, like JSBSim. Not C.** Proper classes following C++ best practice: RAII, clear ownership,
minimal public API.

The coding style follows JSBSim, because FlightBox fits into its ecosystem:

| Item | Rule | Example |
|---|---|---|
| Classes | `FB` prefix (analogous to JSBSim's `FG`) | `FBFlightControl`, `FBRenderer` |
| Methods | PascalCase | `Run()`, `GetLoadFactor()` |
| Members | PascalCase | `LatDeg`, `EngState_` |
| Namespace | `FlightBox` for `core/`, `FlightBox::<Layer>` above it — see below | `FlightBox::Systems` |
| Files | one class per file | `FBName.h` / `FBName.cpp` |
| Getters | inline in the header | — |
| Header guards | yes | — |

JSBSim's LGPL banner is not copied — our files carry our licence.

### Namespaces mirror the layers

Until the `src/` tree was cut into layers there was exactly **one** `namespace FlightBox`, because
there was exactly one place a class could be. There no longer is: `src/` is a stack of twelve
directories with an enforced include order (`sim/tools/verify_layers.py`), and the namespace now says
the same thing to a **reader** that the include rank says to the **build**.

`core/` keeps the root `namespace FlightBox`. Its value types — `FBState`, `FBLog`, `FBGeodesy`, the
incorruptible judges — appear in the signatures of every layer above; nesting them would put a
qualifier on nearly every line and carry no information, since "it is in core" is exactly what "it is
everywhere" already means. Every layer **above** core nests one level, and one only:

`FlightBox::Fdm` · `::Units` · `::Sensors` · `::Weapons` · `::Systems` · `::Pilot` · `::Modules` ·
`::Missions` · `::Clients` · `::Render` · `::World`

A cross-layer name therefore carries its layer where it is **used** — `Fdm::FBFdm`,
`Sensors::FBRadarSystem`, `Units::FBUnitRegistry` — which is the point: the qualifier at the call site
is the layer boundary made visible. Inside its own layer a class is spelled bare, as before.

There is **no second level per airframe**. `FlightBox::Modules::F16` would be the only member of that
level with more than a handful of files, the `FB` prefix already carries the airframe (`FBF16Fcr`,
`FBStoreModule`, `FBGroundModule`), and nothing outside `modules/` names any of those types anyway —
the whole layer is reached through `FBModule*` and the registry's string key. A level that only ever
appears in its own directory buys nothing.

Two consequences, both machine-checked:

* **`using namespace` never appears in a header.** It would leak into every translation unit that
  includes it and delete the qualifier at exactly the call sites the boundary is made of. In a `.cpp`
  it is allowed but rare — today only in the entry-point TUs, where the global `main` and the
  `extern "C"` exports force file scope anyway.
* **The C island is named, not tolerated.** `world/terrain/` (the `osmmesh_*` leaf library),
  `world/FBTerrainLoader.*` (the tile-streaming C ABI), `clients/FBTileWorkerMain.cpp` (the tile
  worker's `extern "C"` exports), `clients/FBSimHost.cpp` and `fdm/em_compat.h` carry an `extern "C"`
  contract or are force-included into vendored C sources. Their namelessness is the contract, so they
  are listed by name with their reason in `verify_layers.py` — and a namespace appearing in one of
  them is an error, the same as a missing one anywhere else.

## `extern "C"`

Only for functions called from JavaScript **by name**. `EMSCRIPTEN_KEEPALIVE` alone is not enough —
mangling breaks exports silently.

The truth is the Makefile's `EXPORTED_FUNCTIONS`, in two link targets: the app
(`fb_toggle_ground`, `fb_set_ground`, `fb_wx_ready`, `fb_wx_failed` in `FBAppWasm.cpp`, plus `main`)
and the tile worker (the ten `fbtw_*` in `FBTileWorkerMain.cpp`). Every one of them is an
`extern "C"` definition, and every one appears unmangled in the built `.wasm`.

The FDM adapter is explicitly **not** such a case and lives in `namespace FlightBox::Fdm`.

## No scattered output

`core/`, `systems/`, `modules/`, `render/`, `world/`, `fdm/`, `units/` **never** emit directly. No
`printf`, no `fprintf`, no `std::cout`, no `std::cerr`.

| Kind | Channel |
|---|---|
| discrete events | `FBLog` (`core/FBLog.h`) — levelled, `tag` + `event` + key=val fields |
| periodic state | `FBTelemetryBus` (`core/FBTelemetry.h`) — time series with a schema |

Exceptions, exhaustively:

- the sink implementations themselves (`clients/FBLogSinks.*`, `clients/FBTelemetrySinks.*`)
- CLI UX in `clients/`: usage, help, argv errors, bootstrap errors before the sinks are set up

Core stays I/O-free, but not formatting-free: `snprintf` into a local buffer is allowed everywhere, a
`FILE*` or `fstream` nowhere.

## Comments

**The purpose of a comment is the non-obvious WHY.** A comment that describes *what* the line below it
does says the same thing in two languages and drifts away — it is omitted. Code and names explain
themselves.

What this project additionally demands: **every number carries its provenance.** A constant is either

- **derived** — then the derivation is stated with it (the formula, not the result), or
- **measured** — then the measurement is stated with it (what, with what, which result), or
- **set** — then it is marked `[SET]` and named as a setting.

A number without one of these three statements is a defect.

> **Decided and implemented** (roadmap R1, commit `f77f1cf`): the derivations live HERE — in the
> `## Knowledge` section of the topic file — and the code carries a one-liner plus a reference. The
> proof that no behaviour changed is the unchanged `sim/tools/strip_comments.py` hash. The existing
> code previously carried these derivations as 15–25-line banners directly in the source.

### Textbook C++, fast shaders — and why that is one goal

> Owner, 2026-08-05: *„das ganze C++-Projekt nach Lehrbuch. Der WebGPU-Part max Performance. Ich denke,
> das widerspricht sich nicht. Der C++-Part muss hervorragend strukturiert und modular sein. Die
> emittierten Shader schnell."*

**The C++ is a compiler; the shader is the program.** A compiler's structure does not run — its output
does. So clean and fast are the same direction here, not a trade.

Two places where they do rub, and the rule for each:

| | Rule |
|---|---|
| **virtual dispatch** | at the seams, never inside a loop over elements. `FBModule` is virtual because it is called once per entity per tick; nothing per-triangle or per-sample may be |
| **data layout** | structure at the seams, **flat data in the loops**. Interfaces stay clean and virtual; what they hand around is contiguous. This is the only genuine conflict, and it is the ECS argument |

Corollary: a RAII wrapper per buffer is right; a wrapper per draw is not.

### One exception: a render stage names its source

The algorithms in this field are solved. Value is in integration, not invention — so a stage that
implements a published technique says which one, in **one line**, at the top:

```cpp
/* Hillaire 2020, "A Scalable and Production Ready Sky and Atmosphere Rendering Technique".
   Deviation: 32 LUT steps, not 40 — bandwidth, see doc/render/visual-target.md §1. */
```

Provenance, not description — the same rule every number obeys. A successor reads it and knows in one
line whether the stage implements the standard correctly or whether someone improvised.

`doc/render/` carries the big picture, never the citation list.

## Structure

- `core/` **never** points into `systems/` or `modules/`.
- Peers never call each other — a module cycles its systems, the systems do not know one another.
- Sensors **write** `FBState`, displays **read** it.
- Weapons get a borrowed `FBWorld` reference, never a global one.
- The renderer is a bolt-on, never a dependency of the physics or the termination logic.
- The renderer's pass topology is a contract: only `FBRenderer` sets pass boundaries, no stage split may
  multiply them.

## Frames

An angle carries the frame it is measured in, and the frame lives in the **type**, not in a comment.

- **World** ("true"): bearing 0 = north, elevation above the local horizontal.
- **Body**: azimuth off the nose (+ right), elevation off the boresight plane (+ above).

Three consecutive rounds each wrote one world angle into one body-frame command. All three compiled:
both frames are `double`, both are degrees, and the two spellings differ by a subtraction that nobody
can see missing. So the subtraction became a constructor and the write became a single door.

- `core/FBBodyAngle` is obtainable in exactly three ways, each naming its provenance:
  `FromTrueBearing(trueDeg, ownYawDeg)`, `FromWorldElevation(worldElDeg, ownPitchDeg)`, and
  `Measured(bodyDeg)` — the one escape hatch, named so that an unearned use is visible at the call site.
  There is no syntax for "just take this double".
- `FBCommandBus::PostAntennaAz/El` take that type and nothing else; they are the **only** place in
  `sim/src` that may name `FBCommandTarget::RadarSlewAz/El` in a post expression.
- `make -C sim verify-layers` prints **`1 antenna-cue poster(s)`** and FAILS on a second, exactly as it
  prints the number of registry readers inside the perception boundary.
- A sensor publishes BOTH pairs where a consumer may need either (`FBRadarContact` and `FBIrstContact`
  carry `BearingDeg`/`ElevAngleDeg` world **and** `AzDeg`/`ElDeg` body), and each field states its frame
  in the struct. `FBRwrThreat` publishes only the body pair, because that is all an RWR ever measures.
- The exact transform both ways is `core/FBGeodesy.h` (`FBEnuToBodyLos` / `FBBodyLosToEnu`) and it is
  spelled once. `FBBodyAngle`'s arithmetic is the antenna-KNOB's — exact at zero roll and on the nose —
  and is used only where the producer has two numbers rather than a vector (a controller's call).

The complete inventory of the tree's angle handovers, with the frame pair and the verdict for each,
is [`sensors.md`](sensors.md) §10.

## A rule nobody can forget to obey

A rule that lives in a `while` head is a rule the NEXT caller does not inherit. The browser proved it:
it wrote itself a second sim loop and left the end condition out, so a CFIT'd F-16 kept being
integrated while the judge's own `monitor KO` line stood in the console. The fix is never a check added
to the second copy — it is that there IS no second copy, and that the compiler says so.

The tree's guarantees are therefore all of one shape: **private with exactly one friend**, or **a type
that has no syntax for the wrong thing**, or **a gate that prints a number**.

- `FBFdm`'s loading constructor — private, one friend (`FBFdmBoot`).
- `FBSystemHealth`'s mutators — private, one friend (`FBDamageModel`). Self-repair does not compile,
  and neither does a forged K.O.
- `FBBodyAngle` — no syntax for a naked double. `FBPilotTuning` — no syntax for an absolute value.
- `units/FBSimUnit`'s tick surface (`Run`, `PublishPose`, `RunMonitors`, `Update*`, `FinalizeMission`,
  `CheckEnvelope`) — private, one friend: `missions/FBMissionSim`, the ONE simulation loop. A client
  cannot step a unit, so it cannot write a loop, so it cannot forget the rule that ends one.
- `FBRunState` — `[[nodiscard]]` on the TYPE: advancing a simulation hands back whether it is over, and
  dropping that value is a compile error in all four builds.
- `make -C sim verify-layers` prints the number of registry readers (**6**), antenna-cue posters (**1**)
  and simulation-loop drivers (**1**), and fails on a seventh, a second, a second.
- `make -C sim verify-guards` compiles eight two-line translation units against the real headers: **six
  must FAIL** and two must succeed. The two that must succeed are not decoration — without them a
  broken include path would "reject" everything and the gate would pass while proving nothing.

An intention is not a structure. Every entry above has been counter-checked by removing the guarantee
and watching the gate go red.

## Architectural style

Build systems, not features. Minimal public API, maximal encapsulation. State machines instead of
boolean flags. Composition over inheritance. Registry/plug-in patterns. Phase-oriented sequences.

Defensive at system boundaries, trusting inside. Fixed capacities and no allocation in the tick path.
No randomness in a deterministic simulation — where a dispersion is needed, it is a model, not a die.
