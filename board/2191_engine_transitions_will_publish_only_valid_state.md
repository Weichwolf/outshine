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
- `setSurfaces` borrows `std::span<const Scenario::Surface>` and copies only into its candidate;
  array input, GPU rejection, retry, rendered pixels and hit targets are covered publicly.
- The headless `declare` path now builds generator geometry and audio occlusion before publishing.
  A generator refusal retains declaration, revision, input, pending geometry and occlusion.
- `Live::Open` and `Engine::declare` detach a replaced `Live` owner only after its successor,
  scroll restoration and generated geometry succeeded. Its destructor therefore cannot clear
  the successor's renderer products; a failed CPU-side build retains the old owner.
- A changed imported asset, variant or clip no longer takes the in-place reuse path. It builds
  the same full candidate as a first declaration. The public missing-asset regression preserves
  the prior declaration and linear frame, then accepts an immediate valid retry.
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

## Next bounded change: replacement and animation history

Source audit: Live::Restands changes Declared_ and clears Held_ before Build succeeds.
Live::Pose replaces PreviousPositionsM_ before Poses/Reshape can fail. This proves
mutation ordering, not that every public caller exposes the failed intermediate state.
Trace each public caller first; reuse its existing candidate where it already isolates Live.

1. Test rejected asset/clip replacement through the public API: declaration, geometry,
   camera, audio and next rendered frame remain A; valid B retries. Include different
   vertex counts. Repair the missing candidate boundary, not a snapshot/restore wrapper.
2. Render history belongs to the last successfully submitted render snapshot, not the
   last advance or bounds measurement. Pose computes candidate current data only.
   Commit history with SceneRenderer's successful Submit, matching topology generation,
   origin and camera. First frame, topology change and owner replacement use current
   positions as previous; failed submit and Measure do not advance history.
3. Reuse SceneState and existing frame/fence owners (WI 2190); retain buffers until GPU
   completion. No per-pose float-to-double copy for GPU history. Double world placement
   remains separate from local float deformation data. No format types in this contract.
4. Tests: two advances without render, failed pose, failed submit then retry, replacement
   with fewer vertices, and bounds measurement between draws. Compare motion data with
   independently computed previous/current transforms; assert no allocation after warmup.

Files: src/engine/Live.cpp, Asset.h, src/render/SceneRenderer.cpp and its SceneState.
Reference: ../SDL at fa2c02b, include/SDL3/SDL_gpu.h submission/fence lifetime.
Commands: make format; make suite SUITE=outshine/include/Outshine; make lint.
Animation-history tests additionally belong under test/outshine/src/render/.
This fix does not require completion of native import migration in WI 2150.

## Acceptance

- [ ] Public transition table documents call ordering, invalidation, thread affinity and errors.
- [ ] Fault injection after validation, `Live::Open`, scroll restoration, generator construction,
      geometry upload and publication retains the previous usable target/world/declaration or
      enters explicit Failed.
- [ ] Retry after every injected failure succeeds; revision changes only on success.
- [ ] Repeat declare, target change and feature toggles show no resource growth.
- [ ] Negative control that publishes before validation is red.
- [ ] Relevant public API tests, target tests and `make lint` are green.
