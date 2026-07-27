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
| Namespace | one `namespace FlightBox` | — |
| Files | one class per file | `FBName.h` / `FBName.cpp` |
| Getters | inline in the header | — |
| Header guards | yes | — |

JSBSim's LGPL banner is not copied — our files carry our licence.

## `extern "C"`

Only for functions called from JavaScript **by name**. `EMSCRIPTEN_KEEPALIVE` alone is not enough —
mangling breaks exports silently.

Today this concerns exactly two symbols: `fb_toggle_ground` and `fb_set_ground` in `FBAppWasm.cpp`. The
FDM adapter is explicitly **not** such a case and lives in `namespace FlightBox`.

## No scattered output

`core/`, `systems/`, `modules/`, `render/`, `world/`, `fdm/`, `units/` **never** emit directly. No
`printf`, no `fprintf`, no `std::cout`, no `std::cerr`.

| Kind | Channel |
|---|---|
| discrete events | `FBLog` (`core/FBLog.h`) — levelled, `tag` + `event` + key=val fields |
| periodic state | `FBTelemetryBus` (`core/FBTelemetry.h`) — time series with a schema |

Exceptions, exhaustively:

- the sink implementations themselves (`app/FBLogSinks.*`, `app/FBTelemetrySinks.*`)
- CLI UX in `app/`: usage, help, argv errors, bootstrap errors before the sinks are set up

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

## Structure

- `core/` **never** points into `systems/` or `modules/`.
- Peers never call each other — a module cycles its systems, the systems do not know one another.
- Sensors **write** `FBState`, displays **read** it.
- Weapons get a borrowed `FBWorld` reference, never a global one.
- The renderer is a bolt-on, never a dependency of the physics or the termination logic.
- The renderer's pass topology is a contract: only `FBRenderer` sets pass boundaries, no stage split may
  multiply them.

## Architectural style

Build systems, not features. Minimal public API, maximal encapsulation. State machines instead of
boolean flags. Composition over inheritance. Registry/plug-in patterns. Phase-oriented sequences.

Defensive at system boundaries, trusting inside. Fixed capacities and no allocation in the tick path.
No randomness in a deterministic simulation — where a dispersion is needed, it is a model, not a die.
