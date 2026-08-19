Type: feature
Area: corpus
Tags: oracle, khronos

**The generator's four animation groups decide pose correctness at two declared frames**

`Animation_Node`, `Animation_NodeMisc`, `Animation_Skin` and `Animation_SkinType` from Khronos'
glTF-Asset-Generator enter the render suite, each case rendered at **frame 0 and one frame inside its
motion**. What they buy is the part of animation a still can decide: **where every vertex ends up when
the pose is applied.**

## What the 24 animated cases already here do not ask

The corpus animates in 24 of 148 cases and each asks one mechanism: node TRS, a skin, morph targets, the
three interpolations, `KHR_animation_pointer`. The generator's groups ask the combinations that break
implementations -- **more than four joint influences on a vertex, holes in a joint chain where nodes are
skipped, one skin shared by several meshes, and skin animation and node animation on one hierarchy at
once**. Every one of those is a wrong *position*, which is exactly what a still shows.

## Two frames and not a sequence, and the reason is a measured cost

**Cycles renders are not cached** -- the owner's ruling, `CLAUDE.md`. The existing corpus renders roughly
114 cases. These four groups are of the order of 40 cases; taken as eight-frame motions that is **320
oracle renders, three times the whole corpus, for one extension**. Two declared stills is 80.

**The frame inside the motion is DECLARED per case and not derived**, because the interesting pose is
where the curve is steep and a runtime that picked the midpoint would sample a rest pose on a case whose
motion is front-loaded. It is one number in a manifest, beside the frame count that is already there.

## What must be true

- [ ] **The generator's output is fetched at a pin like every other upstream**, digest-checked, with the
  fetch cache doing what it already does
- [ ] **A case declares which two frames it is decided at**, and both are compared on the same terms as
  every other case -- covered pixels to the perceptual tail, disagreed pixels to the geometric bound
- [ ] **A refusal is named.** A generator case this engine cannot pose is a declared boundary with the
  capability missing beside it, never a case quietly absent from the count
- [ ] **The count is published beside the existing two**, so *criteria met* and *cases within the picture
  bound* keep meaning what they mean

## What this feature may NOT do

**It may not decide motion.** Smoothness, popping, ghosting and hitching are a moving camera's questions
and belong to the scenario suite; a case here that reported on them would be an instrument answering
outside its domain.
