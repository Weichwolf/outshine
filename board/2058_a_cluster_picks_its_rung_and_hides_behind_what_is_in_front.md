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
