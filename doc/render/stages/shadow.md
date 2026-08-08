# Shadow — the cascaded shadow maps

**Pass:** `ShadowStage` (`sim/src/render/stages/ShadowStage.{h,cpp}`), receiving half `ShadowSample.h`.
Neighbours: [`buildings.md`](buildings.md) (the casters **are** that pass's vertex buffer),
[`terrain.md`](terrain.md) (the other caster), [`ao.md`](ao.md) (the other occlusion
term, and the one that must not double-count this one), [`tonemap.md`](tonemap.md) (where the two are
combined), and [`../renderer.md`](../renderer.md) (the pass topology).

## Spec

| Contract | Why |
|---|---|
| **ONE render pass for all cascades** — depth only, one pipeline, one draw per cascade into a strip atlas | the pass boundary belongs to `Renderer` and a stage split may not multiply passes ([`../renderer.md`](../renderer.md)) |
| the casting half is a stage, the **receiving half is a WGSL splice** every lit surface includes | one binding and one sampler per receiver however many cascades there are |
| casters are **borrowed**, never copied | the same vertex and index buffers the scene pass draws ([`buildings.md`](buildings.md), [`terrain.md`](terrain.md)) |
| a cascade is a VIEW, so it takes **its own cut of every caster's DAG** | a caster drawn at all its levels at once is not merely 2.9× the triangles — the coarse levels stand ABOVE level 0 in places, so the map holds the wrong depth and the surface shadows itself |
| a **plain [0,1] ortho depth**, not the scene's reversed-Z | a comparison sampler carries exactly one compare function, and `Less` is the one that reads naturally for „nearer to the light than the caster" |
| **normal offset**, not a constant depth bias alone | it moves the lookup off the surface by about one cascade texel, which removes acne on grazing lit slopes without the peter-panning a constant bias large enough to do the same would cost |
| the shadow term is the **unshadowed fraction of the direct beam**, and only the direct beam | ambient occlusion darkens sky light, not sunlight ([`ao.md`](ao.md), [`tonemap.md`](tonemap.md)) |

## State

**Built, uncommitted.** `ShadowStage.{h,cpp}` and `ShadowSample.h` exist in the working tree and are
reported untracked by `git status` at the time of this split; the round that builds them is running
concurrently, and **its commit anchor and frame measurement are owed by it** rather than guessed here.

What its own source declares, with the provenance the source carries:

| Quantity | Value | Origin |
|---|---|---|
| cascades | **4** | `[SET]` — „what covers a walker's near metres and a town's few hundred" |
| atlas | **4 × 1024 in a 4096×1024 D32 strip = 16 MB** | `[SET]`, same note; a strip keeps the lookup one-dimensional in the cascade index |
| cascade 0 outer radius | **24 m** | `[SET]` |
| last cascade outer radius | **600 m** | `[SET]` |
| filter | 3×3 PCF | `[SET]` |

**Casters: OSM building prisms AND the resident terrain tiles.** Both ride one pipeline — the two vertex
strides are 32 bytes with position at 0 — and differ only in the per-draw uniform offset that carries the
tile origin.

**A cascade is a VIEW and takes its own cut, of BOTH casters since 2026-08-08.** It borrows the cluster
DAG, not just the buffers, and selects per cascade against **two shadow texels** (`kShadowTauTexels`,
`[SET]`: the
receiver already filters over a 3×3 grid of hardware-PCF taps, so an error under the kernel width cannot
move an edge in the picture). The projection is orthographic, so the metric carries no distance at all —
`err_m / texelM` is one number for the whole cascade — and the cascade box is the cull, sphere against
±(R + r) in both map axes and against the depth span the projection itself clips to.

**The terrain half was drawing its WHOLE buffer per cascade until 2026-08-08 — every level at once —
and that was not only waste.** Measured at the Hochkönig, 1280×720, `kGrid` = 96: **1 093 110 → 474 470**
triangles into the atlas (−57 %), draws 28 → 76. The picture is NOT identical and the difference is the
point: 16 021 pixels change by more than 8 codes, all of them in the near slope (rows 379…719, columns
715…1184), and the mean luminance there rises **90.0 → 147.1**. What went away are the black wedges and
the stripe banding a coarse level cast onto the level-0 surface under it.
`sim/build/out/shadowcut-hochkoenig-before.png` against `-after.png`.

**Measured**, `mods/demo/scene.json`, 1280×720, Dawn/Metal on Apple A18 Pro, one binary with `FB_CULL`:

| | triangles into the atlas | draws |
|---|---|---|
| before | 96 664 | 4 |
| after | **8 550** | 41 |

**−91 %, and the frame is byte-identical** — 0 of 921 600 pixels differ, at four standpoints (1.7 m
pitch 0, 1.7 m pitch +20, 200 m pitch −30, 2000 m pitch −60). The whole reduction is the **cull**: the
τ cut selects level 0 in every cascade, because the building DAG's first coarse level already costs
7.9…11.0 m of error while cascade 3's two texels are 2.34 m. It is built and correct and inert, and it
becomes the binding half the day footprint decimation gives buildings a cheap coarse level
([`../lod.md`](../lod.md) `## State`).

## Gaps

- **The shadow cut is isotropic while the camera's is not.** [`../lod.md`](../lod.md) weighs a vertical
  error by its component ACROSS the view ray; for a directional light the same argument gives
  `sin∠(up, light)`, and a sun near the zenith would then need almost no caster detail at all. Not built,
  not measured.
- **Vegetation casts nothing**, because there is no vegetation ([`../vegetation.md`](../vegetation.md)).
  Foliage self-shadowing is named in [`../visual-target.md`](../visual-target.md) §1.2 as one of the five
  constraints that peak in a forest, and it is the one with no producer at all.
- **No WebGPU virtual-shadow-map implementation was found.** [`../visual-target.md`](../visual-target.md)
  §2.1 lists VSM as the modern replacement for fixed-split CSM on the strength of Epic's system; nothing
  establishes it is reachable within this budget. Until then the four `[SET]` radii above are the policy,
  and they are a *distance* ladder — which is the formulation [`../lod.md`](../lod.md) rules out
  everywhere else. That inconsistency is deliberate and named, not resolved.
- **Every number in `## State` is `[SET]`.** None of the four radii, the texel count or the atlas size
  has been derived from a screen-space criterion or measured against a frame.
- **The per-view τ is inert and therefore unmeasured.** `kShadowTauTexels = 2.0` never binds on this
  scene, so nothing in a frame has yet shown what a coarser or finer shadow cut looks like; the number
  is a `[SET]` with a stated reason and no measurement behind it.
- **No shadow acceptance measurement exists.** Acne, peter-panning and cascade seams are judged by eye
  today; the frame oracle can render the same instant at two cascade configurations and difference them,
  and nothing does.

## Knowledge

Nothing is derived here yet. The bias formulation and the atlas layout are stated in `## Spec` with the
reasoning the source carries; the numbers are in `## State` and are all `[SET]`. The general rule that a
detail's lifetime is a screen-space error and not a radius is derived in [`../lod.md`](../lod.md).
