Type: bug
State: active
Area: engine, include, scenario
Tags: architecture, state, errors
Parent: 2188
Depends:
# Engine transitions publish only valid state

## Contract

`declare`, `assemble`, target changes, UI replacement and configuration setters validate and
build a candidate before publishing it. A rejected operation leaves the last usable declaration,
world, picture, view/input configuration, simulation and audio snapshot intact. An unrecoverable
device failure enters an explicit failed state; it never presents stale pixels as success.

Candidate publication uses a proven nonthrowing transfer. Errors carry an engine error code and
operation context; SDL diagnostics are copied at their boundary. No producer result cache is
assumed: generator output may depend on borrowed, changing provider data.

## Current evidence

- `handleEvent` distinguishes handled, ignored and failed events. Input/UI failures are covered.
- Lens construction is shared by declared and imported cameras; projection validation, recovery
  and its negative control are covered by public tests.
- `assemble` builds a complete simulation candidate and restores the old one when composition
  fails. Entity handles bind camera, audio and triggers; transient body indices do not.
- Target candidates retain the old target on SDL extent/composition/parameter/allocation failure.
- The headless `declare` path now builds generator geometry and audio occlusion before publishing.
  A generator refusal retains declaration, revision, input, pending geometry and occlusion.

## Remaining defect and implementation

The targeted full-declaration path still clears `World` and passes `Picture::Standing` directly to
`Live::Open` before live construction, UI scroll restoration and generated geometry complete.
Any failure can therefore destroy the prior scene. Build `Live`, generated geometry, audio
occlusion, views, bindings and UI state as local candidates. Apply generated geometry to the new
live object, restore scroll offsets, then clear/release replaced world products and swap every
owner. Generation must not mutate live state during preparation.

`Live` ownership and CPU products now remain local through open, scroll restoration and generated
geometry preparation. This is incomplete: `Live::Build` uploads meshes, surfaces and overlays into
the shared `SceneRenderer`, so a failed candidate can still replace GPU-visible state. Stage a
separate renderer resource set or capture/restore all mutated renderer products; prove old pixels
and retry after injected upload, surface and overlay failures.

Inventory each mutator: `offers`, `setRoots`, `setSurfaces`, `declare`, `assemble`, target setup,
save and restore. Unsupported declarations are rejected under 2131. Stable borrowed handles and
nonmoving engine owners are prerequisites where retained references exist.

## Acceptance

- [ ] Public transition table documents call ordering, invalidation, thread affinity and errors.
- [ ] Fault injection after validation, `Live::Open`, scroll restoration, generator construction,
      geometry upload and publication retains the previous usable target/world/declaration or
      enters explicit Failed.
- [ ] Retry after every injected failure succeeds; revision changes only on success.
- [ ] Repeat declare, target change and feature toggles show no resource growth.
- [ ] Negative control that publishes before validation is red.
- [ ] Relevant public API tests, target tests and `make lint` are green.
