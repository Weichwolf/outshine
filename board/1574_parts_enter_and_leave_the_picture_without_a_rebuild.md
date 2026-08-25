Type: feature
State: open
Area: render
Tags: perf, scope
Depends: 1538, 1867

# Parts enter and leave the picture without a rebuild, and glass is a partition rather than a clone

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
what has not changed. Proving test: `harness/outshine/door/ScoreWhatARedeclarationRebuilds`,
which counts `Clients::Live::PlanInits()` over three declarations of one picture --

  FIRST DECLARATION initialised the plan 1 time(s)
  SECOND DECLARATION of the SAME scenario initialised the plan 0 further time(s)
  A SURFACE ADDED over the same picture initialised the plan 0 further time(s)

Negative control, the unconditional teardown restored: 1, 1 and 1, and both CHECKs fail.

What is NOT done, and this item stays open for it: a subject that CHANGES still costs a whole
rebuild, because `Restand` is reached only by the drive path. That is the case the viewer hits
on every switch, and it is the one that matters.
