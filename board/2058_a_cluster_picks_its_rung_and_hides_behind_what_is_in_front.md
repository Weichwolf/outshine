Type: task
State: active
After: 2013
Area: render
Tags: nanite, culling, measured

# A cluster PICKS ITS RUNG, and hides behind what is in front of it

**Benchmark** — Unreal: Nanite's cluster cull is three tests, not one. Frustum, then a HiZ occlusion
test against a pyramid built from the PREVIOUS frame's depth, then a LOD choice per cluster: keep
the cluster whose own error projects under the threshold and whose PARENT's does not, which is what
makes the cut watertight across the ladder. RAGE has no equivalent and does not face the question,
so this is Unreal's answer taken whole. **The reason the choice is not mine**: the parent test is
what stops a crack appearing where two neighbours pick different rungs, and inventing a third
scheme would be inventing that bug.

## What is here already, because half of it is

`DagCluster` (`src/base/spatial/ClusterDag.h`) carries the whole ladder:

    SelfCenter[3]  SelfRadius    the cluster's own bound
    ParentCenter[3] ParentRadius the bound of the group it merges into
    SelfErr  ParentErr           the error each introduces, in metres

`subjectCull.msl` read `spheres[job.x]` as ONE `float4` -- centre and radius -- and tested six
frustum planes. **The error and the parent never crossed to the device.** That half is now done:
`keep()` packs twelve floats a cluster, `CullView` carries `errorPerMetre`, and the rung test
stands behind `if (view.errorPerMetre > 0.0)`.

## AND THE LADDER IS NOT COOKED, which is why the rung test measured INERT

The item said the ladder was "cooked, proven and unreachable". Only the last word was right.
`src/render/Shape.cpp:109` calls **`CookClusters`**, which cuts ONE level and leaves every cluster
a root -- that is the 1 792 rootless clusters this item measured, and it is not a property of the
geometry, it is the function that was called.

`CookDag(positionsM, indices, mostTriangles, mostLevels)` sits beside it in
`src/base/spatial/ClusterCook.h`, is COMPLETE -- it clusters vertices per level, appends the
coarsened positions, rebuilds the index, and writes `ParentCenter`, `ParentRadius` and `ParentErr`
onto every child and `SelfErr` and `Level` onto every parent -- and **`git grep` finds no caller
outside its own declaration and definition.** It is in `test/unreached-baseline`'s 184.

The fourth unwired capability found in one night. **But it is not merely unwired -- it is
INCOMPLETE FOR THIS TREE'S VERTICES, and that is the finding.**

`Cooked` carries `Clusters`, `Index`, `PositionsM` and `FirstOwnVertex`. **Positions and nothing
else.** A coarse level invents vertices at the average of the ones it merges, and this tree's
`ShapePart` carries up to six PARALLEL streams beside the positions:

    PositionsM  Normals  Tangents  Uv  Uv1  Colours

all of them `std::span<const float>` -- VIEWS into buffers the shape does not own. Wiring `CookDag`
as it stands would draw every coarse rung with correct positions and garbage in every other
attribute.

**Unreal does not face this because Nanite owns its vertices outright**: a Nanite mesh is its own
representation built at cook time, not a view over the source buffers, so a simplified rung
carries whatever attributes the cooker chose to average. That is the structural answer and it is
the size of this item, not a line in it.

So the work is, in order: coarsen the ATTRIBUTES beside the positions (the same cell averaging,
normals renormalised); give the shape OWNERSHIP of the coarse vertices rather than a span into
somebody else's array; then grow the part's index range, which Shape.cpp today refuses outright
(`cut.Index.size() != local.size()`).

## THE HiZ HALF LANDED, and the oracle is that it is INVISIBLE

A correct occlusion cull changes the WORK and never the PICTURE, so the test states itself: the
cull must reject clusters AND every `make shots` digest must hold. Either alone proves nothing --
an unchanged digest beside zero rejections is a control that passes because the code does not run,
which is exactly what this cost an hour to notice.

    Heidelberg, one place, one commit
      without occlusion   974 379 indices kept    digest 941e3cf7
      with occlusion      929 835 indices kept    digest 941e3cf7
                          14 848 triangles gone, 4.6 %, and not one bit of picture moved

    Venice   1 307 040 -> 1 303 968 indices, digest 7f387e61 either way
    outshine/places  8 PASS  1 UNPREPARED (CentralPark, board:2068's own refusal)

What it needed, in the order it was needed:

1. **`StageRow::ReadsLastFrame`**, which did not exist. `TopologicalOrderHolds()` proves at compile
   time that nothing reads what a later stage writes, so a depth pyramid -- written after the
   frame is drawn, read before the next one is culled -- could not be DECLARED at all, only hidden
   inside a stage the way `temporalResolve` hides its history. The new edge is excluded from the
   ordering proof and included in the planner's holding and storing, so the pyramid is kept rather
   than pruned.
2. **`Stage::DepthPyramid`**: four levels, each reduced straight from the depth texture over a
   block of 2^(level+1). Independent by construction, because SDL3 orders nothing between
   dispatches inside one compute pass. The cost is written where the constant is: the coarsest
   level reads 16x16 texels a thread, and a single-pass downsampler over threadgroup memory is
   what lifts it.
3. **`subjectCull.msl`** projects the cluster's eight AABB corners through the same matrix the
   frustum planes come from, picks the level whose texel covers the screen box, and rejects when
   the cluster's NEAREST reverse-Z depth is behind the block's FARTHEST.

**`TexelFormat::Handle` with a stride of 0 is never bound.** `Compiled.cpp:290` skips it outright,
so the compute pass never receives it as a read-write buffer and the kernel writes into nothing.
The pyramid read 0.000 for an hour under a perfectly stable digest. `Resource::IrradianceBuffer`
carries the SAME declaration, which is why board:2013's device irradiance reads 0.000.

## What was measured

Shibuya, one frame:

    cull: clusters a frustum would keep     17 255
    subject clusters                        11 454
    subject draws                               49

A city is mostly occluded by its own front row, and 17 255 clusters survive a test that cannot see
that. Nothing measures how many of them are hidden, because nothing asks.

## WHY THE RUNG TEST CHANGES NOTHING: THERE IS NO LADDER TO CHOOSE FROM

    cook: clusters in all                     1 792
    cook: clusters with no parent above them  1 792

Every cluster the cook makes for Shibuya is a ROOT. `CookShape` takes the
`part.IndexCount <= kClusterTriangles * 3` branch and emits one whole cluster with
`ParentErr = kDagRootErr` -- and it takes that branch for every part, because the world arrives as
thousands of SMALL parts rather than a few large ones. The hand-over proves it: 21 504 floats at
twelve a cluster, and the first two clusters' error pair reads `SelfErr 0, ParentErr 3e38`.

So the rung test is correct and inert, and the three thresholds that moved nothing were telling the
truth. A test that keeps a cluster whose parent's error clears the threshold keeps EVERY cluster
when every parent is the root sentinel.

**AND THIS IS NANITE'S PREMISE, NOT A BUG IN THE TEST.** Unreal cuts ONE mesh into clusters and
merges them upward; the ladder is a property of a large connected body. This tree hands the cull a
thousand buildings, each its own part, each under 128 triangles. Nothing to merge, nothing to
choose. The occlusion half of this item does not depend on that and is untouched by it -- a hidden
cluster is hidden whatever rung it is on.

    cull: jobs it swept                   73 706
    cull: indices the subject cull kept   6 422 028

## WHERE IT STANDS, and the LOD half is IN and NOT PROVEN LIVE

The ladder crosses now: `ClusterSpheres` packs twelve floats a cluster -- own centre and radius,
parent centre and radius, own error and parent error -- and `subjectCull.msl` reads three `float4`
and applies Unreal's test, keeping a cluster when its own projected error is at or under a texel and
its parent's is not. The field of view comes out of the frustum planes the cull already computes:
the top and bottom normals of a symmetric frustum meet at the vertical field, so `-dot` between them
is its cosine and no second declaration of a number the matrix already carries is needed.

**AND THE PICTURE DOES NOT MOVE, IN ANY OF THREE SETTINGS.** The projection factor was scaled by
100, by 1/100 and left alone; Shibuya answered `aff2f732` every time and drew 73 706 clusters in 5
draws every time. A test whose threshold can move by four orders of magnitude without moving one
pixel is not running.

    the control that was WRONG      x100, expecting coarser -- it keeps FINER, because a larger
                                    projection makes the parent's error clear the threshold and the
                                    leaf win. Reasoning about the direction after the fact is what
                                    caught it
    the control that should work    x1/100, expecting coarser
    both                            aff2f732, 73 706 clusters, 5 draws

Ruled out so far: the stages run (`subjectCull, took 0.033 ms`), the draw DOES bind the compacted
list (`culled = cut && batch.JobCount > 0` and `cut` holds), the cook DOES build levels and sets a
child's `ParentErr` per level, and the packing IS twelve floats. What is not yet known is whether
`errorPerMetre` reaches the kernel non-zero -- the measure that would say so was added and did not
print, because the edit that added it did not match its anchor and returned silently. THAT is the
next step and it is one measurement.

## What will be true

- [ ] the error and the parent bound cross to the device beside the centre and radius
- [ ] a cluster is kept iff its OWN projected error is under the threshold and its PARENT's is not,
      so exactly one rung of every chain draws and the cut is watertight
- [ ] a HiZ pyramid is built from the previous frame's depth, and a cluster whose nearest depth is
      behind the pyramid's farthest depth over its projected extent is rejected
- [ ] the CPU issues a CONSTANT number of dispatches whatever the scene holds -- it already does for
      the frustum cull, and neither new test may change that
- [ ] `cull: clusters the occlusion rejected` and `cull: clusters that picked rung N` are published,
      because a cull nobody can count is a cull nobody can debug
- [ ] the picture does not move on a scene with no occlusion, and Shibuya's does not move in any way
      an eye can see -- a cluster that is hidden contributes nothing, so rejecting it is invisible
      or it is a bug

## What this does NOT cover

The software rasteriser. Nanite's other half exists because a cluster smaller than a pixel is faster
to rasterise in a compute shader than to hand a triangle pipeline; CLAUDE.md's own goal says
**Nanite WITHOUT the software rasteriser**, so that half is refused here and stays refused.

Streaming. Nanite pages clusters in and out under a budget; this tree has no eviction for subject
geometry at all (`grep Evict src/render/` finds none) and that is its own item.

## WHY EVERY CLUSTER IS A ROOT, and it is one call

`cook: clusters with no parent above them` reads **23 207** on Shibuya against
`cook: clusters in all` **23 207**. Every cluster is a root, so there is exactly ONE rung and no
selection can choose between rungs -- which is why this item's second box cannot be built as it
stands.

The reason is `Shape.cpp:109`: it calls `CookClusters`, the FLAT cut. **`CookDag` exists, is
complete, builds levels, and `grep -rn "CookDag" src/ test/` finds its declaration, its definition
and NO CALLER** (board:2082).

## AND WHAT BLOCKS THE SWITCH IS OWNERSHIP, measured rather than assumed

`CookDag` merges vertices into cells and APPENDS the averaged positions -- `firstMade =
out.PositionsM.size() / 3`. A parent cluster therefore indexes vertices that do not exist in the
source, so every attribute stream must be coarsened alongside the positions, and the shape must OWN
what it coarsens.

**The glTF subject path already owns it.** `Shaped.cpp:16-30` narrows all six streams into the
`ShapeStore` and the parts span into it. The WORLD path does not: `FillFrom(const Geometry&)` points
`made.PositionsM` at the Geometry's own arrays.

So the order is now measured rather than guessed:

1. `CookDag` takes the attribute streams and averages them the way it averages positions
2. the SUBJECT path switches to it and is proven on the Khronos corpus, where the data is owned
3. the WORLD path pays for ownership -- Shibuya hands over 587 MB, so that copy is roughly 100 ms
   against a reshape that is 15 ms today, and Unreal builds Nanite's structure OFFLINE for exactly
   this reason. Whether this tree rebuilds the world's clusters every time is the question that
   step asks, and it is a different item's to answer
