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
any generator needs terrain. The same unchecked access exists in
`Engine::generated`. Normal builds can appear to work because the reference
is often unused. This is undefined behavior, including for valid glTF input.

## Decision

Make absent ground an explicit value at this boundary. `GroundStack` exposes
a nullable const query; generator preparation accepts that pointer. If no
ground is open, `Generators::Request::Ground` is null, as the generator API
already allows. With ground open, a sampler delegates to the same query.
Neither declaring nor generating may call the reference-returning `Ground()`
without first establishing `Opened()`. Do not open an artificial world merely
to satisfy a glTF-only declaration or fabricate zero-metre heights.

## Acceptance

1. A public glTF-only declaration and a groundless generated asset declare
   and assemble without UBSan. A probe generator sees null ground; a terrain
   declaration gives it a usable sampler. Failed input keeps the previous
   declaration and error.
2. The previously red validator case
   `khronos/validator/glb-length-mismatch~sanitised` no longer reports a null
   `GroundStream` reference. Run the relevant public tests, `make format` and
   `make lint`; the full validator batch may reveal separate importer faults.
