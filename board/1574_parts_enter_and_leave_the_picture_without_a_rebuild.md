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
