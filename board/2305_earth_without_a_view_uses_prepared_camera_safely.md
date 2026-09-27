Type: defect
State: active
Architecture: ready
Parent: 2191
Depends: 2268
Priority: P0
Area: engine, ground, render
Tags: camera, ground, headless, assertion

# Earth without a declared view uses a valid prepared camera

## Evidence

A public Engine test that declares and assembles Earth with no `Views` aborts
at `CameraState::Override()` (`assert(Override_.has_value())`). Groundless
scenes without a view assemble normally. `src/engine/Laying.cpp` reads
`Picture.Standing->Watching()` twice for structure projection and terrain
refinement without checking `Watched()`. `WorldFocus.cpp` already uses the
ground origin when no view exists; `Advancing.cpp` selects `Aimed()` when no
override exists. Empty `Views` is a valid declaration, not an instruction to
invent an explicit camera.

## Contract and implementation

`RuntimeScene` owns the auto-framed prepared camera. World streaming may use
its `Aimed()` value for render-detail estimates after the scene has stood;
it must call `Watching()` only when `Watched()` is true. Geographic focus for
an unviewed Earth remains the declared origin. Do not mutate the scenario,
silently add a view, or use an uninitialized camera. Audit every `Watching()`
call in `src/engine` and keep explicit-view behavior unchanged. If auto framing
cannot supply finite projection parameters, reject preparation with a useful
error rather than asserting or issuing unbounded detail requests.

## Falsifiable acceptance

- Public no-view Earth declaration, assembly and advancement do not assert;
  the origin and prepared projection yield finite bounded requests. A later
  explicit view changes the projection source without changing world identity.
- Groundless replacement after an Earth scene remains valid (WI 2268);
  a deliberately absent renderer/invalid projection fails explicitly.
- Run the direct public test with sanitizers, relevant streaming tests,
  `make format` and `LINT_JOBS=2 make lint`.
