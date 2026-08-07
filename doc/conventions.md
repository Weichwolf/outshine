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
   Measurements beat inspection; the control loop ([`build-and-ops.md`](build-and-ops.md)) is
   how a claim about behaviour gets settled.
3. **Update State and Gaps, and add one line to [`journal.md`](journal.md)** (commit, what it built,
   what it measured).
4. **Rejected approaches stay in Gaps** — with their measurements. A measured failure is knowledge;
   deleting it means someone re-runs the experiment.

Two consequences worth stating: there is no second list of open work anywhere (no `TODO.md`, no
trailing "open points" per file — Gaps is the one place), and `CLAUDE.md` is touched only when a
session-start fact changed, kept under 100 lines.

## Language

**C++17. Not C.** Proper classes following C++ best practice: RAII, clear ownership, minimal public API.

| Item | Rule | Example |
|---|---|---|
| Classes | **no prefix**, PascalCase | `FlightControl`, `Renderer` |
| Methods | PascalCase | `Run()`, `GetLoadFactor()` |
| Members | PascalCase, trailing underscore when private | `LatDeg`, `EngState_` |
| Namespace | `outshine` for `core/`, `outshine::<Layer>` above it — see below | `outshine::Systems` |
| Files | one class per file, **the file name IS the class name** | `Name.h` / `Name.cpp` |
| Getters | inline in the header | — |
| Header guards | yes | — |

> **The `FB` prefix is gone.** It existed only because there was no namespace, and it was removed across
> 286 files on 2026-08-06. Two exceptions are deliberate: `world/terrain/` is a lowercase C library
> (`terrain.h`, `mesh.h`, `geo.h`), and **`FBWX` survives as the name of a file format**, which is not a
> class name at all.

### Namespaces mirror the layers

`src/` is a stack of directories with an enforced include order (`sim/tools/verify_layers.py`), and the
namespace says the same thing to a **reader** that the include rank says to the **build**. The gate
checks both halves.

`core/` IS the root `namespace outshine`. Its value types — `Log`, `Geodesy`, `Mat4` — appear in the
signatures of every layer above; nesting them would put a qualifier on nearly every line and carry no
information, since "it is in core" is exactly what "it is everywhere" already means. Every layer
**above** core nests one level, and one only:

`outshine::Units` · `::Render` · `::World` · `::Clients`

A cross-layer name therefore carries its layer where it is **used** — `Units::UnitRegistry`,
`Render::Renderer` — which is the point: the qualifier at the call site is the layer boundary made
visible. Inside its own layer a class is spelled bare.

There is **no second level.** A level that only ever appears in its own directory buys nothing.

Two consequences, both machine-checked:

* **`using namespace` never appears in a header.** It would leak into every translation unit that
  includes it and delete the qualifier at exactly the call sites the boundary is made of. In a `.cpp` it
  is allowed but rare — today only in the entry-point translation units, where the global `main` and the
  `extern "C"` exports force file scope anyway.
* **The C island is named, not tolerated.** `world/terrain/` (the leaf mesh library),
  `world/TerrainLoader.*` (the tile-streaming C ABI), `clients/TileWorkerMain.cpp` (the tile worker's
  `extern "C"` exports) and `clients/SimHost.cpp` carry an `extern "C"` contract or are a standalone C
  binary. Their namelessness **is** the contract, so they are listed by name with their reason in
  `verify_layers.py` — and a namespace appearing in one of them is an error, the same as a missing one
  anywhere else.

## `extern "C"`

Only for functions called from JavaScript **by name**. `EMSCRIPTEN_KEEPALIVE` alone is not enough —
mangling breaks exports silently.

The truth is the Makefile's `EXPORTED_FUNCTIONS`, in two link targets. Measured 2026-08-07:

| Target | Exports |
|---|---|
| the app | `_main`, `_malloc`, `_free` — **and nothing else.** The ground-toggle and `/wx`-callback exports went with the code that answered them |
| the tile worker | the `fbtw_*` set in `clients/TileWorkerMain.cpp` — `open`, `build`, `verts`, `nverts`, `gridverts`, `err`, `origin`, `mips`, `mipbytes`, `ts`, `release` |

Every one of them is an `extern "C"` definition, and every one appears unmangled in the built `.wasm`.

## No scattered output

Every layer below `clients/` **never** emits directly. No `printf`, no `fprintf`, no `std::cout`, no
`std::cerr`.

| Kind | Channel |
|---|---|
| discrete events | `Log` (`core/Log.h`) — levelled, `tag` + `event` + key=val fields |
| periodic state | `TelemetryBus` (`core/Telemetry.h`) — time series with a schema |

Exceptions, exhaustively:

- the sink implementations themselves (`clients/LogSinks.*`)
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

> **Decided and implemented** (commit `f77f1cf`): the derivations live HERE — in the `## Knowledge`
> section of the topic file — and the code carries a one-liner plus a reference. The proof that no
> behaviour changed is the unchanged `sim/tools/strip_comments.py` hash.

### Metric, decimal, throughout

> Owner, 2026-08-05: *„bitte alles metrisch Dezimalsystem."*

Project-wide, and it is not cosmetic. Standards frequently carry **two parallel, non-identical**
requirement sets: API 650's shell table gives 5 mm *or* 3/16 in for the same diameter band, and 3/16 in
is 4.7625 mm. Those are two numbers, not a conversion.

Measured consequence, from the first asset built under this rule: computed in inches the tank **requires**
an intermediate wind girder; computed in SI it does not (8.861 m transformed against H1 = 9.461 m). Mixing
the systems produced a component that exists for one reason and is dimensioned by the other.

So: **read the SI column, carry it through every table.** Where a source is imperial only, convert once at
the boundary and record the conversion as the number's origin.

### Textbook C++, fast shaders — and why that is one goal

> Owner, 2026-08-05: *„das ganze C++-Projekt nach Lehrbuch. Der WebGPU-Part max Performance. Ich denke,
> das widerspricht sich nicht. Der C++-Part muss hervorragend strukturiert und modular sein. Die
> emittierten Shader schnell."*

**The C++ is a compiler; the shader is the program.** A compiler's structure does not run — its output
does. So clean and fast are the same direction here, not a trade.

Two places where they do rub, and the rule for each:

| | Rule |
|---|---|
| **virtual dispatch** | at the seams, never inside a loop over elements. `DrawStage` is virtual because it is called once per pass per frame; nothing per-triangle or per-sample may be |
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

- `core/` **never** points into any layer above it.
- Peers never call each other.
- A producer **writes** a published block, a consumer **reads** it. Never both.
- A layer that needs the world gets a **borrowed** reference, never a global one.
- The renderer is a bolt-on, never a dependency of the physics or the termination logic.
- The renderer's pass topology is a contract: only `Renderer` sets pass boundaries, no stage split may
  multiply them.

## Frames

An angle carries the frame it is measured in, and the frame is stated where the angle is produced.

- **World** ("true"): bearing 0 = north, elevation above the local horizontal.
- **Body**: azimuth off the nose (+ right), elevation off the boresight plane (+ above).

Three consecutive rounds of the previous era each wrote one world angle into one body-frame command. All
three compiled: both frames are `double`, both are degrees, and the two spellings differ by a
subtraction that nobody can see missing. **The lesson is that the conversion belongs in a type with no
syntax for "just take this double"** — the type that carried it is deleted with its callers, and
whatever produces a body-referenced angle next has to re-earn the shape.

The exact transform both ways is `core/Geodesy.h` (`EnuToBodyLos` / `BodyLosToEnu`), it is spelled once,
and it stands.

## A rule nobody can forget to obey

A rule that lives in a `while` head is a rule the NEXT caller does not inherit. The browser proved it:
it wrote itself a second sim loop and left the end condition out, so an actor that had already flown
into the ground kept being integrated while the judge's own `monitor KO` line stood in the console. The
fix is never a check added to the second copy — it is that there IS no second copy, and that the
compiler says so.

The tree's guarantees are therefore all of one shape: **private with exactly one friend**, or **a type
that has no syntax for the wrong thing**, or **a gate that prints a number**.

**What is still enforced today is exactly one thing:** `make -C sim verify-layers` reads every
`#include` against the layer matrix, prints the number of `units/UnitRegistry.h` readers per category,
and fails on one more.

> **The compile-time guarantees lost their subject on 2026-08-07** and are named rather than quietly
> dropped: the monotone health register with its single friend, the friend-locked entity tick surface,
> the `[[nodiscard]]` run state, the private loading constructor that was the single state writer. All
> four classes are deleted, and `verify-guards` — the gate that proved them by trying to break them —
> went with them. **Whatever spawns and steps a body under the new format must re-earn those shapes, or
> the anti-cheat argument has a hole in it** — see [`body-format.md`](body-format.md).

An intention is not a structure. The layer gate is counter-checked by removing the guarantee and
watching it go red; what the block quote names is not checked by anything today.

## Architectural style

Build systems, not features. Minimal public API, maximal encapsulation. State machines instead of
boolean flags. Composition over inheritance. Registry/plug-in patterns. Phase-oriented sequences.

Defensive at system boundaries, trusting inside. Fixed capacities and no allocation in the tick path.
No randomness in a deterministic simulation — where a dispersion is needed, it is a model, not a die.
