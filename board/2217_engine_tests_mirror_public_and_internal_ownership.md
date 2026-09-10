Type: task
State: active
Parent: 2188
Area: tests
Tags: architecture, iteration
Depends:

# Engine tests mirror public and internal ownership

## Problem
`test/outshine/conventions` mixes API contracts, import internals, numerical
algorithms, generators and rendering. Directory names do not identify ownership.
The runner also grants implementation include paths to public API tests.

## Decision
Mirror public headers under `test/outshine/include/` and implementation components
under `test/outshine/src/`, with a directory per tested header/component.
Keep full place scenarios under `test/outshine/integration/places/`.
This follows the project's existing include/src boundary; no external framework
or replacement test oracle is needed. Existing independent cases remain intact.
Keep MVT sanitizer arms, device validation and process-allocation instrumentation.
Resolve build profiles by component, not by a second list of individual tests.
Update runner selection, includes, claims and live documentation references.
Public test translation units receive no implementation include directories.

## Acceptance
- Every existing C++ case appears exactly once under its new responsibility.
- Existing specialized execution arms remain discoverable and execute.
- Moved tests build and run; unavailable integration inputs are reported explicitly.
- No conventions bucket or stale executable test paths remain.
- `make format` and `make lint`, including clang-tidy, run after migration.
- No engine or image behavior changes are intended; assertions remain unchanged.
