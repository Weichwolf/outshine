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
- [ ] The BVH refits, or rebuilds only the region that changed, off the frame path.
- [ ] The glass clone dies: transmissive draws are a batch partition over ONE residency, and
      the cloned catalogue row goes with it.
- [ ] A relay's frame cost is measured before and after over a declared drive.

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

- [ ] the picture holds MORE THAN ONE subject, and the refusal above is deleted on the day it
      does. The proving case is already written to see both ends: it passes when two subjects
      draw twice the batches of one, and it passes when the engine refuses by name -- so the
      day the feature lands, the case reads the feature rather than the refusal.
      proof: outshine/door/ScoreWhatASecondSubjectDoes
