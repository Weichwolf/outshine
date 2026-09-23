Type: defect
State: active
Architecture: ready
Parent: 2191
Depends:
Priority: P0
Area: engine, scenario, ground
Tags: state, nullability, sanitizer

# Groundless declarations do not dereference a ground stream

## Defect and evidence

`make test` on 2026-09-23 reached Khronos validator cases with UBSan and
reported `GroundStack::Ground()` binding a reference to a null `GroundStream`
in `Engine::declare` (`Declaring.cpp:625`). A glTF-only scenario has no
ground provider and never opens `GroundStack`; declaration still passes
`Stack.Ground()` into headless generator preparation before asking whether
any generator needs terrain. The same unchecked access exists in the private,
unreferenced `Engine::generated`. Normal builds can appear to work because
the reference is often unused. This is undefined behavior, including for
valid glTF input.
The supported contract is broader: scenarios may be entirely groundless and
still use native geometry, simulation, audio, UI and generators. Earth/OSM
streaming is an optional capability, not the engine's root state.

## Decision

Make absent ground an explicit value at this boundary. `GroundStack` exposes
a nullable const query; generator preparation accepts that pointer. If no
ground is open, `Generators::Request::Ground` is null, as the generator API
already allows. With ground open, a sampler delegates to the same query.
Declaration preparation must not call the reference-returning `Ground()`
without first establishing `Opened()`. Remove the unused private `generated`
helper. Do not open an artificial world merely to satisfy a groundless
declaration or fabricate zero-metre heights. Audit
other `Ground()`/`Pool()` call sites for a preceding ground-state guard;
headless space and 2.5D scenarios must preserve their own coordinates.
An old Earth stack can remain resident across declaration replacement, so
absence is determined by the active scenario first, then stack availability.
`sampleHeight` must refuse and `loading`/`loadProgress` must report no ground
work for a groundless scenario even if previous terrain remains cached.

## Acceptance

1. Public glTF-only, native-geometry and simulation-only declarations, plus
   a groundless generated asset, declare and assemble without UBSan. A probe
   generator sees null ground before the ground stack is opened; when it is
   open, the sampler delegates to that query. A groundless scenario advances
   without terrain requests, height queries refuse and loading reports no
   terrain work; failed input keeps the previous declaration.
2. The previously red validator case
   `khronos/validator/glb-length-mismatch~sanitised` no longer reports a null
   `GroundStream` reference. Run the relevant public tests, `make format` and
   `make lint`; the full validator batch may reveal separate importer faults.
