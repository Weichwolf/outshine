Type: architecture
State: open
Architecture: ready
Parent: 2188
Depends:
Priority: P1
Area: base, ui, engine, scenario, test
Tags: javascript, ui, determinism, ownership

# ECMAScript drives transactional HTML/CSS UI and bounded world commands

## Problem and present evidence

Outshine intends HTML for UI structure, CSS for presentation and JavaScript for authored
behaviour. The present path is not that contract. `Ui::Markup`, `Stylesheet`, `Layout` and
`Painting` implement a tested subset. `Script::Program` is a small private language despite
test262-shaped tests. On every pointer action `Declaring.cpp` concatenates the surface programme,
the `data-action` text and a semicolon, parses the whole source again and runs it against
`ActionHostAdapter`. There is no persistent ECMAScript realm, module lifecycle, DOM mutation,
event loop, saved script state or shared contract for UI, gameplay and minds. Do not describe
this as general JavaScript support.

## Binding decision

- Keep the engine-owned, documented HTML/CSS subset. Outshine is not a browser and offers no
  network, navigation, cookies, WebGL or implicit browser globals. WPT cases prove only the
  named supported layout/style features.
- Replace the private language with a maintained embeddable ECMAScript runtime through one C
  adapter. Evaluate QuickJS and its maintained compatible fork from pinned local Git clones;
  select the implementation that passes the required test262 subset, builds without C++
  exceptions and meets the budgets below. Do not expand `Script::Program` toward ECMAScript.
- `ScriptRuntime` owns the VM and allocator. `ScriptRealm` owns globals, compiled modules,
  handlers, pending events and failure state. UI, world/gameplay and each scheduled mind use
  separate realms with explicit capabilities; no realm owns or borrows renderer, physics,
  generator or mutable world objects.
- Authoritative state remains in versioned Engine components/tables. Scripts read immutable
  tick snapshots and emit typed commands using validated entity/resource handles. Commands are
  bounded, ordered deterministically and applied atomically at a simulation tick boundary.
  Rejected commands change neither world nor UI. Save/replay stores authoritative state,
  clock, queued external answers and module identity; arbitrary VM heaps are not savegames.
- `UiDocument` owns parsed markup, styles and script-visible DOM state. `UiSession` owns layout,
  paint, scroll and renderer publication. Script DOM commands build a candidate document;
  parse/layout/paint/GPU replacement publish together or roll back together. Event handlers
  are compiled once and dispatched by identity. Never concatenate source text per event.
- Script execution runs on the simulation owner, never the render thread. Time comes from the
  fixed simulation clock. Randomness comes from an injected seeded stream. Timers and jobs use
  the Engine scheduler with deterministic ordering. Promises enter only with a bounded job queue
  whose drain point and overflow result are specified; no wall clock or hidden worker callback.
- ECMAScript exceptions stay inside the adapter and become structured `std::expected` failures;
  the C++ runtime remains exception-free. A realm has explicit Ready, Running, Suspended and
  Failed states. Failure preserves the last published world/UI and remains observable.

## Budgets and host boundary

Initial enforceable ceilings per realm: 8 MiB heap, 100,000 interpreter/interrupt-budget units
per dispatch, 1,024 pending events, 1,024 pending jobs, 256 emitted commands and 32 nested host
calls. These are starting limits, not performance claims; benchmark on target-class hardware
and revise with numbers. The adapter must interrupt nontermination and account allocation.
Capabilities are typed interfaces grouped by UI, entity query, world command, audio and
diagnostics. Missing capability, stale handle, invalid argument, overflow and host refusal are
distinct script-visible errors. Strings crossing the boundary are owned or consumed in-call;
no VM pointer survives a call.

## Implementation order for the coding agent

1. Pin local runtime reference clones and record revision, licence, build size, cold creation,
   retained bytes and bounded-loop interruption. Add a negative control proving the private
   interpreter cannot satisfy a selected standard case. Commit the selected adapter decision.
2. Introduce runtime/realm RAII with allocator, interrupt, exception translation and no host
   capabilities. Run scripts only through focused adapter tests; normal tests use no shell tool.
3. Add typed value conversion and a fake command sink. Prove capability denial, stale handles,
   deterministic command order, rollback and every budget. No Engine pointer in the adapter.
4. Replace pointer-source concatenation with compile-once surface modules and named handlers.
   Preserve current `data-action` behaviour during migration, then reject inline source actions
   once handler references are available. Failed compile/dispatch preserves published pixels.
5. Add candidate DOM commands and transactional `UiDocument` -> `UiSession` publication.
   Add keyboard/focus/text input only with explicit event order and ownership.
6. Connect world/gameplay realms at the fixed tick boundary. Then make 2141 and 2136 consume
   this runtime; do not give NPCs or quests a second scripting implementation.
7. Remove `Script::Program`, its misleading claims and obsolete tests after all consumers move.

## Acceptance

- A module is compiled once, retains realm-local variables across events and dispatches pointer,
  keyboard and tick handlers in a specified deterministic order without source concatenation.
- Same seed, state, events and external-answer log yield byte-identical commands and snapshots;
  replay after save/load yields the same UI pixels and world state.
- Infinite loop, heap/event/job/command overflow, thrown value, denied capability, stale handle
  and failed DOM/GPU publication are bounded and preserve the last committed state.
- Two realms cannot observe or mutate each other's globals. Destroy/recreate releases retained
  memory within the measured ceiling; no callback fires after realm or Engine destruction.
- Required selected test262 cases and named WPT subset pass with effective negative controls.
  Public interaction tests use the Engine API; adapter tests use a fake host. `make lint` passes.
