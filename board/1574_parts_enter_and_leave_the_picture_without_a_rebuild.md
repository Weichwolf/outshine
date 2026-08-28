Type: feature
State: active
Area: render
Tags: perf, scope
Depends: 1538, 1867

# Parts enter and leave the picture without a rebuild, and glass is a partition rather than a clone

**Benchmark** — Unreal: `FScene` adds and removes primitives without rebuilding; translucency is its own pass. RAGE: entities enter and leave the draw list. **Both agree** — a part entering must not rebuild the picture.

`SubjectDraw::SetMesh` re-uploads every vertex stream, re-uploads the indices and rebuilds the
whole CPU triangle BVH on ANY content change, and a drive calls it at every relay — allocation,
lock, disk and an unbounded block firing on the frame path at once. When the plan holds glass
everything doubles: `Renderer.h` mirrors every `Set*` into a complete clone of the stage, which
is what `{Stage::SubjectsTransmissive, ...}` (src/render/plan/RenderCatalogue.h:268) declares as
a second stage in the plan.

What ships elsewhere: geometry resident and shared, entities added and removed incrementally
(RAGE drawable dictionaries and entity pools; Unreal's persistent scene). The engine's own
store-and-handles ladder IS that design; only the render path cannot receive it.

## What will be true

- [ ] Parts are added and removed against persistent residency — a relay uploads only what
      arrived, and the frame path allocates nothing.
- [x] The BVH refits rather than rebuilds, and takes nothing while it does.
      Unreal refits a skeletal mesh's bounds and physics BVH per frame and rebuilds only on a
      TOPOLOGY change; RAGE updates a model's bound hierarchy in place. **Both agree**, and the
      reason is CLAUDE.md's frame-path invariant: a rebuild is O(triangles) with a fresh
      allocation, a refit is O(nodes) into storage that already stands. The predicate as filed
      asked for the refit to be OFF the frame path as well -- that is the wrong bound and is not
      taken: neither engine moves it off, because a bounded term with no allocation is exactly
      what the frame path is allowed to carry. What matters is that it is bounded and takes
      nothing, and that is what is now measured.
      `SetPose` already called `Visibility_.Refit`, which writes into the standing `Tris_` and
      `Nodes_` and allocates nothing. **That was a reading of the code, not a measurement**, and
      the instrument to measure it did not exist: `Heap::Tagged` has counted bytes per tag all
      along and nothing outside the library could read the count. The engine now publishes
      `heap taken under <tag>` per written tag, cumulative, and the consumer differences it --
      the engine aggregates and does not compute statistics.
      Measured: `OVER 8 POSED FRAMES  mesh-bvh took 0 byte(s)`.
      proof: outshine/door/ScoreWhatAFramePathTakes, over a subject the case writes itself with a
      translation track -- a still subject never reaches `SetPose` and would read zero for the
      wrong reason, which is why the BUILD's own count is checked non-zero first.
      negative control: a copy of the structure inside the refit's tag reads 768 bytes and takes
      the case to FAIL -- outshine/door 35 PASS 1 FAIL.
      **And the same measurement found what this predicate is not about**: `render-frame` takes
      12160 bytes over those eight frames, 1520 a frame. board:1943 owns that and now has the
      number and the instrument.
- [x] The glass clone dies: transmissive draws are a batch partition over ONE residency.
      **The before-number, through the door.** `Renderer` held `Subjects_` and `Glass_` as two
      full `SubjectDraw`s, each with its OWN residency, and `Renderer.h` forwarded every upload to
      both -- `SetMesh`, `SetPose`, `SetMaterials`, the BVH. A new `device bytes` counter on the
      subject stages read, for ABeautifulGame:

          subjects              173566016 device byte(s)
          subjectsTransmissive  173566016 device byte(s)

      173.6 MB held twice on an 8 GB device, and the pose re-uploaded twice a frame.
      Unreal: `FScene` holds ONE copy and the translucency pass draws the mesh batches whose
      material is translucent, from the same vertex buffers. RAGE: one draw list, passes filter
      it. **Both agree that a pass is a partition and never a copy.**
      `Glass_` now BORROWS `Subjects_`'s residency and skips the uploads; its batches, pipelines
      and uniforms stay its own, because those are what makes it a different pass.
      After: `subjects 173566016`, `subjectsTransmissive 0` -- and the zero is honest rather than
      cosmetic, because a borrowing stage owns no bytes and the sum across stages is now the
      device total instead of double it.
      proof: harness/khronos/glTF 444/444 -- the corpus draws transmissive scenes and the picture
      does not move, which is the only control that matters when two passes start sharing buffers;
      outshine/door 35/35; gate GREEN.
- [x] A relay's frame cost is measured BEFORE, over a declared drive, and the after is what
      the predicate above has to beat. `apps/bench --steps 48 --heap --cache
      /tmp/outshine-drive-cache --size 320x180`, over `apps/driver/src/f31.scenario`:

          STEPS                 48
          WITH A PICTURE        113.9 ms   2.373 ms/step
          WITHOUT ONE            94.4 ms   1.967 ms/step
          THE PICTURE COSTS     17.1% of the drawn run

          HEAP  untagged        2 549 600 byte(s) per step
          HEAP  render-frame        1 328
          HEAP  subjects               80
          HEAP  subjectsTransmissive   80

      **2.5 MB a step, and it is not the renderer**: the three render tags together are 1488
      bytes. The untagged remainder is the relay -- the world re-composed as the drive moves, and
      `SetMesh` re-uploading every stream on any content change, which is what the predicate above
      exists to end. The instrument is board:1574's own (`heap taken under <tag>`, differenced by
      the consumer) and it did not exist when this predicate was written, which is why the
      predicate had no number for as long as it stood.
      proof: apps/bench is where a client is measured (`test/gate.sh` names it), and the numbers
      above are its output verbatim.

## Measured 2026-08-25, and the first cost bound now holds

`Engine::Declare` reset `S_->Standing` and called `Live::Open` unconditionally, so every
declaration re-initialised the whole render plan -- pipelines, passes and every resource behind
them -- even when the declaration was identical to what stood. The evidence it left behind was
`wasScrolled`, saved across the teardown and handed back afterwards: state being carried by hand
over a rebuild that should not have happened.

`Live::Redeclare(surfaces)` and `Live::Restand(built, carried)` already existed and `Declare`
reached neither. The capability was present and unreachable.

`Declare` now diffs the declaration it would hand over against the one that stands and reuses
what has not changed. Proving test: `outshine/door/ScoreWhatARedeclarationRebuilds`,
which counts `Clients::Live::PlanInits()` over three declarations of one picture --

  FIRST DECLARATION initialised the plan 1 time(s)
  SECOND DECLARATION of the SAME scenario initialised the plan 0 further time(s)
  A SURFACE ADDED over the same picture initialised the plan 0 further time(s)

Negative control, the unconditional teardown restored: 1, 1 and 1, and both CHECKs fail.

The case that matters is closed too: a subject that CHANGES. `Live::Restands(stands, variant,
animation)` swaps the file behind a standing picture -- the render plan survives because
`Plan_ == nullptr` already guarded its init and only the teardown was destroying it, and the
plan is rebuilt only where the new subject needs a different one, which is when it animates and
the old did not. Proving test: `outshine/door/ScoreWhatASubjectSwapRebuilds`, two
minimal glTF triangles written into the nest --

  FIRST SUBJECT   read 1 asset(s), initialised 1 plan(s)
  SWAPPED SUBJECT read 1 further asset(s), initialised 0 further plan(s)
  SWAPPED BACK    initialised 0 further plan(s)

Three subjects in one picture: 3 reads, 1 plan. The read count is half the measurement -- a swap
that read nothing would mean the case measures nothing.

Negative control, the subject folded back into the sameness test so a changed subject takes the
full path: 1 further plan per swap, and both CHECKs fail.

**The diff that decides all of this is hand-written, and the next field added escapes it.**
`SamePicture` (src/engine/Engine.cpp:514-526) compares 21 members of `Clients::Declaration`
one by one; `SameStand` adds three and `SameShows` seven. Add a member to `Declaration` and
every one of them keeps saying *the same picture*, so a declaration that CHANGED is reused and
the defect is a stale frame, silently. `Declaration` is an aggregate of comparable members: a
defaulted `operator==` is the whole of it in C++20, and where a member genuinely must not
count, the exclusion is spelled once rather than the inclusion twenty-one times. Until then a
`static_assert(sizeof(Clients::Declaration) == N)` beside the diff is the minimum, so the day a
field is added the build stops.

What is NOT done, and this item stays open for it: parts ENTER and LEAVE. Today the picture
holds one subject and a swap replaces it. A scenario that declares five bodies and drops one
still has no verb, and the entity store is where that belongs (board:1896, board:1897).

**The line that says so is one call.** `Engine::State::Draws` carries
`Ticking.Freestanding.front()` -- the FIRST freestanding body and no other -- and
`Live::Carry` then places every part of that one subject on that one transform. A scenario
declaring two standing bodies draws one of them, and nothing refuses: the second is simply
absent from the picture, which is the quietest kind of wrong.

**This blocks board:1957's last predicate**, which asks for a DELTA instead of the boundary
diff. A delta over a one-row table is a line of code rather than an architecture, so that
predicate waits here and says so rather than being written twice.

**MEASURED, and the silence is now a refusal.** `Scenario::Subject()` returns the FIRST glTF
asset and ignores the rest; `Live` stands over that one document. A scenario naming two:

    one subject draws 1 batch(es), two draw 1

`Engine::Declare` refuses that now, by count and with a reason -- the others would be accepted,
counted and never drawn, and a frame that renders half of what was declared looks finished. That
is a step BACKWARD from the benchmark and it is deliberate: Unreal's `FScene` holds a proxy per
primitive and RAGE puts every entity on its node's draw list, so neither has anywhere for a
second subject to be dropped FROM. Until the picture can hold two, saying no is the honest half
of the answer, and accepting a declaration and doing nothing with it is the dishonest one.

- [x] EVERY freestanding body is carried into the picture, not the first, and N copies of one
      mesh cost ONE draw call with N instances. **The before-number, measured through the door**:
      `apps/bench --scene DamagedHelmet --steps 12 --bodies 16` read `the simulation integrates 16
      freestanding bodies` beside `1 draw(s) 15452 tri 1 placement(s) 1 differ`. Sixteen declared,
      sixteen integrated by `Falls()`, ONE carried -- the prose above had named this since the item
      was filed and no predicate carried it.
      Unreal: one `FPrimitiveSceneProxy` per primitive, the TRANSFORM per instance in `FGPUScene`,
      so N copies are one draw over an instance run. RAGE: every entity on its node's draw list,
      one geometry submitted once with a matrix per entity. **Both agree and neither has a
      first-body case**, so duplicating the geometry per body -- the other way to make sixteen
      helmets -- would have been a third answer and is not taken.
      What it took: `SubjectProxy` gained an INSTANCE dimension (`row = part * instances +
      instance`, so a part's instances are consecutive and one draw covers them), `DrawBatch` a
      count, and `Live` a body per instance. **The shader needed nothing** -- it already read
      `rows[2u * iid]` from `[[instance_id]]`, which Metal counts from `baseInstance`, so the
      capability was in the tree and only the CPU side had no way to ask for it.
      Measured after: `1 draw(s) 247232 tri 16 placement(s) 16 differ` at sixteen, `1 draw(s)
      61808 tri 4 placement(s)` at four -- the call count holds while the triangles multiply.
      **Three counters were lying and are fixed in the same change**: `Triangles` did not multiply
      by instances and `Placements` read `ModelSlot + 1`, so the bench would have reported sixteen
      instances as one body's work. A rate is Arbeit / Zeit and the Arbeit has to be right.
      proof: outshine/door/ScoreWhatManyBodiesDraw
      negative control: restoring `Carries(Ticking.Freestanding.front(), ...)` takes it to FAIL --
      outshine/door 34 PASS 1 FAIL.
- [x] the picture holds MORE THAN ONE subject, and the refusal is deleted. `one subject draws 1
      batch(es), two draw 2` -- the case reads the FEATURE rather than the refusal now, with no
      edit, exactly as it was written to. Negative control: disabling the append reads
      `two draw 1`.
      **The mechanism existed and nothing reached it**: `Gltf::Subject::Append` was complete,
      tested by `outshine/content/ScoreWhatAnAppendKeeps`, and called by no production code.
      What was needed was a LIST where `Declaration::Stands` held one path, and the append placed
      AFTER `Pose(0)` rather than before -- posing rebuilds the geometry from the file and was
      overwriting the join.
      **What it costs is now board:1943's**: two identical subjects are two draw calls, because
      `DrawList::SameState` has `ModelSlot` in the merge key and `DrawBatch::Draws` -- an
      instance count -- is never raised above 1.
      proof: outshine/door/ScoreWhatASecondSubjectDoes
