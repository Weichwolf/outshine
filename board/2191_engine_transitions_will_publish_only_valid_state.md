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
- `Live::Open` and `Engine::declare` detach a replaced `Live` owner only after its successor,
  scroll restoration and generated geometry succeeded. Its destructor therefore cannot clear
  the successor's renderer products; a failed CPU-side build retains the old owner.
- `SceneRenderer` now builds a move-only `SceneState` candidate containing frame, plan and world
  content. Full declarations publish it only after geometry and overlays succeed; a generated-world
  submit failure retains prior linear pixels and retries. Frame-owned cull and shadow stages rebind
  their subject address after the content move.

## Remaining defect and implementation

The targeted full-declaration path builds `Live`, generated geometry, audio occlusion, views,
bindings and UI state locally, then detaches and replaces the old owner only after the candidate
is complete. Generation must not mutate live state during preparation. Its remaining failure
boundary is GPU-visible state, not CPU ownership.

`Live` ownership and CPU products remain local through open, scroll restoration and generated
geometry preparation. `SceneState` stages all GPU-visible declaration products under WI 2223;
snapshot/restore remains rejected because coupled GPU ownership cannot prove complete restoration.

The remaining direct geometry mutators call `Live::SetGeometry` on the published owner. They need a
cloneable native world-input candidate, not a CPU move-and-rollback; WI 2224 owns `setGeometry`,
pending geometry and streaming ground. Surface-only redeclaration, `Restands`, `offers`, `setRoots`,
save and restore remain separately inventoried.

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

`setGeometry` beschrieb den GPU-Fehlerpfad fälschlich als nichttransaktional. Der Code bereitet
eine `Live`- und Renderer-Kandidatin vor, verwirft sie bei Fehler und veröffentlicht erst danach.
Die öffentliche Doku nennt jetzt die erhaltene alte Szene und Audio-Occlusion; Dokumentationstest
und 189/189 tidy sind grün. Der vollständige Übergangstisch und Fault-Injection bleiben offen.
