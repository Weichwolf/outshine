Type: bug
State: active
Architecture: ready
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

`setGeometry`, pending geometry and streaming ground already prepare native world candidates
under WI 2224. Do not reimplement them as direct live mutation. Remaining work is whole-product
proof and the transition audit below; 2224 owns its concrete implementation order.

## Transition audit after the current world-publication work

For each public mutator in `include/Outshine.h`, trace its implementation in `src/engine/`
and record: admissible state, borrowed inputs, published owners, fallible preparation,
nonthrowing commit, invalidated references, thread affinity and failure result. Keep that
contract next to the public declaration; do not build a second descriptive state machine.
Order: surface replacement/Restands, offers/setRoots, save/restore, then remaining setters.
Only combine transitions whose ownership and commit boundary actually coincide.

Geometry and audio occlusion must derive from the same candidate geometry. Prepare both
before swapping either; fault injection must observe the previous audio snapshot as well
as pixels. Camera/animation updates retain valid placements and current frame parameters.
New declaration resets world-local handles; same-world replacement preserves their identity.
Frame/target resources belong to WI 2222, GPU world ownership to 2223, world inputs to 2224;
do not make these nested owners compete for renderer cleanup.

Each implementation step gets a test under `test/outshine/include/Outshine/`: valid A,
rejected B after a late operation, A still usable, valid B succeeds on immediate retry.
Use internal GPU injection only to trigger the public operation's failure. Document actual
coverage; a helper test does not prove the full public transition.

## Acceptance

- [ ] Public transition table documents call ordering, invalidation, thread affinity and errors.
- [ ] Fault injection after validation, `Live::Open`, scroll restoration, generator construction,
      geometry upload and publication retains the previous usable target/world/declaration or
      enters explicit Failed.
- [ ] Retry after every injected failure succeeds; revision changes only on success.
- [ ] Repeat declare, target change and feature toggles show no resource growth.
- [ ] Negative control that publishes before validation is red.
- [ ] Relevant public API tests, target tests and `make lint` are green.
