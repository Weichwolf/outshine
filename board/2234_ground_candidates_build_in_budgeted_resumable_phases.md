Type: defect
State: active
Architecture: ready
Parent: 2105
Depends:
Priority: P0
Area: engine, world, rendering
Tags: streaming, realtime, ownership

# Ground candidates build in budgeted resumable phases

## Evidence

The historical ground operation returned after a 15 s deadline because the deadline
was checked before synchronous work. The newer floor-contact result is green (WI 2231);
the missing per-unit bound is still an architectural gap. Lattice's recorded ingestion/
classification timeout supplies the next regression fixture. Re-measure the current
path; historical timing alone does not identify today's longest stage.

As of 2026-09-21 the cold floor fixture is green at 8.58 s and Lattice at 10.19 s.
Height-page handles now validate against their owner and duplicate terrain producers are
rejected. Lattice's former fixed and adaptive refinement paths no longer emit the same
tile. These end-to-end bounds are green; per-stage continuation and tail measurements
remain the work of this WI.

The candidate now retains corridor products and advances separately through Corridors,
Earthworks, Water, Geometry and Publication. A normal `advance()` performs at most one of these;
preload may flush several within its explicit bound. Cold results after the split are
8.62 s Floor and 10.54 s Lattice. This establishes a real interruption boundary while
keeping publication atomic. Corridors and Earthworks still operate on whole bounded
candidate inputs; measure their tails before deciding whether either needs an inner cursor.

Warm Floor completion costs measured on 2026-09-21 are Corridors 36.8 ms, Earthworks
22.8 ms, Water 0.01 ms, Geometry 210.2 ms and Publication 0.31 ms. Geometry contains a
1.15 ms class upload and 209.1 ms `RuntimeScene::SetGeometry`; 207.5 ms of that is frame-plan
setup. Clustering, packing and mesh upload together remain below 1 ms. Ground classes now
upload into the candidate renderer rather than mutating the published renderer. WI 2223 owns
the remaining frame/world boundary defect. Corridors and Earthworks need inner continuation
only after their tail distributions and maximum inputs are recorded.

WI 2223 entfernt nun Frame-Neubau für identische `PlanSpec`s. Der Floor-Kandidat ergänzt
weiterhin den aus Geometrie abgeleiteten Shadow-Stage und ändert damit den Plan tatsächlich:
warm 176.0 ms Geometry, davon 173.3 ms Plan-/Frame-Aufbau, gegenüber 0.50 ms Publication.
Dieser Planwechsel braucht einen begrenzten Frame-Resource-Build oder ein früher stabiles
Stage-Set; die Arbeit darf nicht zurück in Publication verschoben werden.

## Decision

Extend the existing private GroundWorldCandidate owner; do not create a parallel world
transaction. It retains one coherent input revision, candidate CPU/GPU products and
phase-local continuation cursors. Suggested phase names describe actual operations:
Prepare, Patchwork, Classify, Routes, Earthworks, Water, Geometry, Validate.
Advance returns Pending, Ready, Rejected or Cancelled. Ready transfers a complete product
to the existing engine-thread publication owner; publication is not a second state machine
phase with a competing Engine::State swap. The active world remains unchanged until that commit.

Every synchronous unit has bounded input size and a measured tail cost. Splitting a large
function into named phases does not make it budgeted: continue within the longest phase
by tile/row/batch, preserving algorithmic dependencies and stable reduction order. Never
split a topology operation at an arbitrary index if its invariant requires the whole set.
A unit too expensive for the budget requires a continuation or a bounded worker product.

Retain immutable input owners until worker completion; revision changes cancel the old
candidate rather than splicing new data into its completed phases. Reserve candidate
memory before dispatch using existing budgets (2228); defer when unavailable, preserve A.
GPU lifetimes follow existing SDL owners (2190). Do not require an upload fence solely
for later GPU sampling; CPU reuse/readback needs its proper completion contract (2235).

## Bounded implementation

1. Measure per-stage elapsed time and candidate peak bytes in Engine::State::Grounds (src/engine/Laying.cpp),
   src/engine/GroundWorldCandidate.h, GroundPublication.h and
   src/render/scene/TerrainTileUpload.h.
   Use completion snapshots, no periodic logs. Keep an uninterrupted control path in
   tests as an oracle for identical native products, not a second production algorithm.
2. Make the longest measured stage resumable using the existing candidate. A controlled
   test pauses/resumes it after each safe unit; world A's revision, contact data, geometry
   and pixels remain unchanged. Do not first extract every stage into speculative classes.
3. Test stale input, cancellation while work runs, late GPU submission failure and retry.
   Commit B once through the existing owner; failure retains A. Continue additional stages
   only where measurements show unbounded work. Integrate admission under WI 2233 later.

WI 2231's unfinished public proof is not a code prerequisite. Reuse its candidate fixture
when available; avoid duplicating engine-owned publication or exposing public test hooks.
Expected visual result: unchanged completed world; smoother preparation during movement.

## Acceptance and commands

- [ ] Interrupted and uninterrupted builds have identical final native products; no partial
      terrain/building/contact revision is visible. Deliberate early publication fails.
- [ ] Per-unit time distribution, maximum unit size and candidate peak memory are recorded;
      deadlines checked before and after work report overruns honestly.
- [ ] Unchanged floor-contact and Lattice 15 s limits pass with contact checks intact.
- [ ] make format; make suite SUITE=outshine/integration/places/ScoreAFootprintStandsOnALevelFloor;
      make suite SUITE=outshine/integration/places/ScoreTheLatticeMeetsItselfAtALevelBoundary;
      added candidate continuation cases through make suite; make lint.
