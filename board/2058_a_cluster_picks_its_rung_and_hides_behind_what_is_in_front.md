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

`subjectCull.msl` reads `spheres[job.x]` as ONE `float4` -- centre and radius -- and tests six
frustum planes. **The error and the parent never cross to the device.** So the ladder is cooked,
proven and unreachable: the commonest defect this tree names, and the third time today.

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
