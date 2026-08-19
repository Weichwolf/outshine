Type: feature
Area: corpus
Tags: oracle, khronos
Depends: 1451

**The generator's five animation groups decide pose correctness at two declared frames**

`Animation_Node`, `Animation_NodeMisc`, `Animation_SamplerType`, `Animation_Skin` and
`Animation_SkinType` from Khronos' glTF-Asset-Generator enter the render suite, each case rendered at
**frame 0 and one frame inside its motion**. What they buy is the part of animation a still can decide: **where every vertex ends up when
the pose is applied.**

## What the 24 animated cases already here do not ask

The corpus animates in 24 of 148 cases and each asks one mechanism: node TRS, a skin, morph targets, the
three interpolations, `KHR_animation_pointer`. The generator's groups ask the combinations that break
implementations -- **more than four joint influences on a vertex, holes in a joint chain where nodes are
skipped, one skin shared by several meshes, and skin animation and node animation on one hierarchy at
once**. Every one of those is a wrong *position*, which is exactly what a still shows.

## What the upstream holds, read at the pin rather than assumed

[MEASURED] at `3d99767e9a67fbfe109f0d298c1e8d909bcac9db` (2026-07-13), `Output/Positive`:

| group | cases |
|---|---|
| `Animation_Node` | 6 |
| `Animation_NodeMisc` | 9 |
| `Animation_SamplerType` | 3 |
| `Animation_Skin` | 12 |
| `Animation_SkinType` | 4 |
| | **34** |

**`Animation_SamplerType` was not in the first statement of this item and belongs**: it carries the
interpolation kinds. Each case is a `.gltf` of about 2.8 KB beside a `.bin` of about 1 KB, and **each
group upstream carries a readme whose markdown table holds one row per case naming exactly what
varies** --
`Target: Translation, Interpolation: Step`. The criterion is therefore READ from upstream rather than
written here, which is what makes 34 cases a tool's work instead of 34 writing tasks.

## TWO THINGS BLOCK IT AND BOTH WERE FOUND BY TRYING

**`board:1451` is a real `Depends:` and not a courtesy.** A prepared case records a digest over every
`.py` under `test/harness/`, so adding a vendor's fetch step invalidates every existing case -- and
re-preparing the picture corpus is hours of Cycles. **Importing 34 cases would cost a full re-render of
148 before a single new picture existed.** That is measured on the board already; it is named here
because it decides the order.

**THE CAMERA CANNOT BE DERIVED BY A TOOL, AND THAT IS THE LARGER OBSTACLE.** A manifest declares its
camera as `source: gltf` (an index into the file's own cameras) or `source: manifest` (seven numbers).
The generator's models carry no camera, so every case needs the seven -- and the framing rule takes the
subject's **world bounds over the union of the declared frames** (`board:1433`), which means posing the
node hierarchy and sampling the animation. **A Python importer computing that would be a second spelling
of the flattener and the sampler**, which is the one thing `CLAUDE.md` forbids outright.

*The way out is already half-built*: `prep/in_blender_render.py` has the scene loaded, posed and
animated when it calls `build_camera`, so the bounds are one function away -- and Blender's own importer
is a flattener nobody has to write. **The four framing constants would then be the only thing stated
twice**, and `ADerivedCameraIsTheFramingRuleAndNotAQuotation` already refuses a disagreement. Doing that
touches the preparer, which is why `board:1451` comes first and not second.

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
