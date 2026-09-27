Type: defect
State: done
Architecture: ready
Parent: 2191
Depends: 2268
Priority: P0
Area: engine, ground, render
Tags: camera, ground, headless, assertion

# Ground detail requires a valid camera before preparation

## Evidence

A public Engine generator-redeclaration test that assembles a viewless Earth
scene aborts at `CameraState::Override()` (`assert(Override_.has_value())`).
The debugger locates the call in `BeginGroundSheetRefinement`. A separate
advance of an empty viewless Earth scene already returns an explicit
auto-framing error; it is not the assertion. `src/engine/Laying.cpp` reads
`Picture.Standing->Watching()` twice for structure projection and terrain
refinement without checking `Watched()`. `WorldFocus.cpp` already uses the
ground origin when no view exists; `Advancing.cpp` selects `Aimed()` when no
override exists. Empty `Views` is a valid declaration, not an instruction to
invent an explicit camera.

## Contract and implementation

`RuntimeScene` owns the auto-framed prepared camera. Ground preparation uses
its `Aimed()` value for render-detail estimates when no override exists;
it must call `Watching()` only when `Watched()` is true. Geographic focus for
an unviewed Earth remains the declared origin. Do not mutate the scenario,
silently add a view, or use an uninitialized camera. Audit every `Watching()`
call in `src/engine` and keep explicit-view behavior unchanged. If auto framing
cannot supply finite projection parameters, reject preparation with a useful
error before posting work rather than asserting or issuing unbounded detail
requests. A viewless Earth with no renderable geometry has no valid lens and
may fail assembly; adding a valid view must permit a new assembly.

## Falsifiable acceptance

- A public no-view Earth declaration followed by assembly rejects the missing
  projection with an error, not a signal. A subsequent explicit view assembles
  and its prepared lens supplies finite detail estimates.
- Groundless replacement after an Earth scene remains valid (WI 2268);
  a deliberately absent renderer/invalid projection fails explicitly.
- Run the direct public test, relevant streaming tests, available sanitizer
  coverage, `make format` and `LINT_JOBS=2 make lint`.

## Result

Both ground-detail call sites select `Watching()` only for an explicit view
and otherwise read the prepared `Aimed()` camera. They reject a nonfinite eye,
nonpositive frame height or invalid lens before calculating focal pixels or
posting detail work. The old public redeclaration sequence now returns a
camera error for viewless empty Earth instead of aborting; a valid Geodetic
view then assembles, and a groundless generator subsequently receives null
ground. The streaming pacing test and its validation variant pass. The public
test profile is not sanitizer-instrumented; the separate UBSan validator in
WI 2268 covers the original nullable-ground defect, not this camera path.
