Type: feature
State: open
Parent: 1995
Depends: 1992
Area: render
Tags: benchmark, target, gpu-driven

# A pass is ONE indirect draw whose count the GPU wrote

**Benchmark** — Unreal: a Nanite pass issues an indirect draw whose argument buffer the culling compute stage filled, so the CPU never learns how much survived. RAGE: the device consumes a prepared draw list. **Taking Unreal** — an indirect draw is the only shape where the CPU's work is O(1) in the scene, and it is the end state the other four items exist to make possible. **Reference**: Haar & Aaltonen, *GPU-Driven Rendering Pipelines*, SIGGRAPH 2015 — `MultiDrawIndirect` fed from a GPU-written argument buffer, and the observation that the CPU cost becomes a constant.

## What

The subject pass records one indirect draw. The draw count, the instance count and the first
index come from a buffer the compute stage wrote.

## Why

This is where "no CPU term scales" stops being a claim and becomes a measurement. Every step
before it reduces the CPU's per-item work; this one removes the last of it — the CPU stops
knowing how many things there are.

Today: `SubjectDraw.cpp:828` issues `SDL_DrawGPUIndexedPrimitives` inside a loop over batches, one
call per batch. Board:1989 merges identical batches, board:1992 decides which survive — and after
both, the loop is still a CPU loop over a GPU-decided set, which is the last place a readback
would sneak back in.

## How

The culling stage writes an indirect argument buffer; the pass records
`SDL_DrawGPUIndexedPrimitivesIndirect` against it. Whether SDL_GPU exposes a multi-draw variant
or one indirect draw per material class is a question for the device layer and is answered
before the code moves, not during it.

**What would show this was wrong**: if the argument-buffer round trip costs more than the draws
it replaces at this scene size, then a 720p engine with tens of subjects does not need what an
engine with tens of thousands does. Frame time over `apps/driver` before and after is the number,
and board:1989 takes the baseline so there is something to compare against.

- [ ] the culling stage writes an indirect argument buffer
- [ ] the subject pass records ONE indirect draw per material class, not one per batch
- [ ] the CPU's draw-call count over a drive is CONSTANT in the number of subjects -- published
      as a measure so a regression is visible

**board:1989 hands this item its last structural predicate.** `SameState` cannot drop `ModelSlot`
in the direct path, and the reason is measured: `ModelSlot` is the PART index and every part owns
its index range, so merging two parts into one batch would issue ONE instance over both ranges and
give both the first part's placement row. Merging across slots needs a per-draw index offset,
which is what an indirect draw carries. What board:1989 leaves ready for it: the vertex uniform is
per PASS and holds nothing per-instance -- `{viewProj, prevViewProj, lightFromWorld, shift,
prevShift}`, 56 floats, every one a property of the VIEW -- so there is nothing left to thread
through. The proof it owes is board:1574's: two identical subjects read `two draw 1` while
`linear_channels_differing_between_renders` stays at zero.

**MEASURED, and this item now has a scene that shows why it exists.** `apps/bench --all` over
Khronos's own, thirty steps each, subject stage only:

    scene            draws   triangles   tri/ms
    DamagedHelmet        1      15 452   1 015 977
    ABeautifulGame      49   1 500 224  15 695 511
    Sponza             103     262 267   1 279 351
    VirtualCity        167       8 383       8 723

**VirtualCity draws 8383 triangles in 167 draws** -- fifty triangles a draw, and a rate two orders
of magnitude below every other scene. That is the draw-call-bound case, and it is the one an
indirect draw is for: the cost is the CALL, not the geometry. Without the rate beside the count it
is invisible, because 167 draws and 0.961 ms both look unremarkable alone.

So this item has its before-number: whatever the indirect draw does to VirtualCity's tri/ms is
what it is worth, and the other three scenes are the control -- they must not get worse.

## board:2001 tried the shortcut and MEASURED why there is none

The shortcut was: if two parts' placement rows are EQUAL, give them one slot and let the existing
merge condition do the rest -- no indirect draw needed. Sponza reads `103 placement(s) ... 1
differ`, so the rows really are equal there, and the shortcut took Sponza to 25 draws and
ABeautifulGame to 21, exactly their material counts, with khronos/glTF 444/444.

It is still wrong, and the reason is the one this item already states, seen from the other side.
`SubjectProxy::Places` decides the merge at STAND. `Placed_` is EMPTY at that moment -- measured,
`placed=0` in both builds -- so every row is the identity fallback and every row is equal. The
merge therefore collapses everything, and the placements arrive AFTERWARDS, per frame, through
`MovePlacement`. Sixty-four crates standing in sixty-four places:

    without the dedupe   batches=64  rows=2048 floats  ring opened 51680  door 33/33
    with the dedupe      batches=1   rows=  32 floats  ring opened 43616  door 30 PASS 2 FAIL

The frame writes 64 rows either way, because rows are sized from `Placed_`. So the merge shrank
the BUDGET, which is taken from the batches' slot count at stand, and not the DEMAND, which is
taken from the placements at frame time -- 8064 bytes apart, and the overflow that had no
explanation for two attempts.

**Unreal never merges on transform equality, and this is why.** A transform is per-INSTANCE data
living in `FGPUScene`; it changes every frame and is not knowable when the batch is formed.
`FMeshBatch` merges on STATE -- material, vertex factory -- and never on a value. A merge that
reads a transform is a merge that reads data which does not exist yet.

So the residual is exactly the per-draw index offset this item already names, and there is no
cheaper route to it: a draw spanning parts with different transforms needs each vertex to find its
own row, which is `FGPUScene`'s primitive id in the vertex factory. Nothing before that helps.

## THE CEILING ON "ONE DRAW" IS THE DEVICE LAYER, AND IT IS MEASURED

A material is free and a TEXTURE is not, and the difference is why this item says *per material
class* rather than *per pass*.

**Free**: baseColour, metalness, roughness are a row in a buffer read by ONE shader. `kMaterialSlots`
is `1 << 24`, so a thousand materials cost a thousand rows and no pipeline change.

**Not free**: `SubjectDraw.cpp:863` calls `SDL_BindGPUFragmentSamplers(into.Pass, 0, images,
kSubjectImages)` per surface. A rebind breaks the batch. Sponza draws **103 times for 25
materials**, and this session measured what that rebind costs: sorting MATERIAL before DEPTH took
the subject stage from 4.19 to 8.09 million tri/ms at an UNCHANGED draw count. The whole win was
eight bindings happening 25 times instead of 103.

**And SDL_GPU has no bindless.** Its header declares `num_samplers` as a fixed count per shader;
neither "bindless" nor descriptor indexing appears anywhere in it. A texture cannot be an index
through this layer, so ONE draw for the frame is not reachable and one per material class is the
honest ceiling. This item already said so and now carries the reason.

**AND THE CEILING BINDS THE CORPUS, NOT THIS ENGINE'S OWN WORLD.** Measured after the paragraph
above was written: the driven world holds **4 surfaces and 4 images** over 10 draws. Sponza's 25
and VirtualCity's 167 are VENDOR assets. outshine is procedural and its materials are parameters,
so "one indirect draw per material class" is, for its own content, four classes -- which is one
draw and three. The ceiling bounds how well this engine draws OTHER PEOPLE'S textured assets, and
CLAUDE.md now says where the budget goes instead: geometry, light and shadow, as close to
texture-free as the work allows.

**The question that follows, named rather than answered here.** Metal 4 has argument buffers, so
the ceiling is SDL's abstraction and not the hardware. Whether a Nanite-class pipeline can be
finished on a device layer without bindless is a question about the ARCHITECTURE, not about this
pass, and it is answered before anyone starts here rather than discovered halfway through. The
measurement that settles it: the draw count this item drives to a constant is bounded below by the
number of material classes, so the number to compare is `surfaces` -- 25 for Sponza, 15 for
ABeautifulGame, 167 for VirtualCity. If a scene's material classes alone exceed the budget, no
amount of indirect drawing saves it and the device layer is the finding.
