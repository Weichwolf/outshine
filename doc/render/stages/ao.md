# Ao — screen-space ambient occlusion

**Pass:** `AoStage` (`sim/src/render/stages/AoStage.{h,cpp}`). The **composite is not a pass** — it lives
in [`tonemap.md`](tonemap.md), which already reads every scene pixel.
Neighbours: [`shadow.md`](shadow.md) (the direct-beam occlusion this must not double-count),
[`atmosphere.md`](atmosphere.md) (the sky irradiance this darkens) and
[`../renderer.md`](../renderer.md) (the pass topology).

## Spec

| Contract | Why |
|---|---|
| a **separate pass**, because it samples the depth texture that was an attachment a moment ago | a texture cannot be attachment and sampled resource in the same pass |
| **half resolution into an R8**, bilinear on the way out | the AO field has no detail above ~1 m at the radius used, and the bilinear upsample is what removes the sample noise a per-pixel spiral would leave |
| it darkens **sky light, not sunlight** | direct occlusion is the shadow map's job; applying AO to the sun would darken the same photon twice |
| the direct fraction rides in the **HDR target's alpha**, written by every lit surface | it is the one channel that is otherwise unused, and it avoids a second colour attachment — which matters because `maxColorAttachmentBytesPerSample` is 32 B by default ([`../visual-target.md`](../visual-target.md) §2.1) |
| the sense of that alpha is „1.0 means AO does not apply" | every stage that has not been converted writes 1.0 already, so an unconverted surface fails safe |
| **GTAO is the target form**, not classic SSAO | closed-form horizon integral, matches a ray-traced ground truth; Jimenez et al. 2016 report **0.5 ms on PS4 at 1080p** for GTAO+GI at a half-res occlusion buffer — cheaper than what [`../visual-target.md`](../visual-target.md) §1 budgets, on weaker hardware, and pure compute |

## State

**Built as a sampled-disc SSAO.** For three weeks it wrote a **diagnostic flag instead of its result**:
the fragment ended `vec4f(ao * 1.0e-9 + select(0.0, 1.0, orient > 0.0), abs(orient), 0.0, 1.0)`, where
`orient = dot(nrmA, p0)` and `nrmA` is constructed two lines above so that this dot product is never
positive. The red channel — the only one the composite reads — was therefore **0 on every pixel the
pass ran on**, and the `1.0e-9` was what kept the compiler from stripping the AO the shader had just
computed. This is the *second* instance of the same failure class in as many rounds; the first was in
`TonemapStage` (commit `de70f98`).

**What it cost, measured at the reference scene** (`FB_TONE_PROBE=-16,4`, the curve as a ruler, so a
PNG byte is `log2 L`): the composite is `scene.rgb * mix(ao, 1, alpha)` and a fully sky-lit surface
writes `alpha = 1 - (1 - kGroundBounce) = 0.12`, so `ao = 0` multiplied it by 0.12 = **−3.06 EV**. A
shaded facade in the Hameln Altstadt read **log2 L = −7.330** (sRGB 13,13,13 — neutral grey under a
blue sky, which is the tell) and now reads **−4.193**: **+3.137 EV**, against the 3.06 EV the
composite alone predicts. In the reference scene itself the defect is confined to the distant town:
**67 pixels of 691 200** moved by more than 20 codes, all on the horizon building band at y ≈ 358–362,
from 44 to 125.

| Quantity | Value | Origin |
|---|---|---|
| shaded-facade gain from the fix | **+3.137 EV** | measured, tone-probe ruler, Altstadt street frame |
| predicted from the composite alone | +3.06 EV | derived, `−log2(mix(0, 1, 0.12))` |
| ground pixels at display code 0, after | **0** of 691 200 | measured, three frames |
| frame time, p99 over four walk speeds | 18.29 / 19.76 / 19.69 / 20.16 ms | measured, `walkbench.py`, against the same-session baseline binary 19.37 / 20.79 / 20.43 / 20.30 ms — inside run-to-run spread, and the shader lost two operations |

What its own source declares, with the provenance the source carries:

| Quantity | Value | Origin |
|---|---|---|
| world radius | **0.9 m** | `[SET]` — „the contact scale a walker reads: a wall foot, a kerb, the seam where a prism meets the ground" |
| samples | 16 | `[SET]` |
| resolution | half, R8 | `[SET]`, with the reason in `## Spec` |
| minimum screen extent before the effect is skipped | 2.5 px | `[SET]` |
| occlusion floor | 0.25 | `[SET]` |

**This is not GTAO.** It is a spiral of depth samples with a normal-offset guard, which is the 2015-class
method [`../visual-target.md`](../visual-target.md) §2.1 names as the thing to replace.

## Gaps

- **GTAO is specified and SSAO is built.** The replacement is named with a source and a measured cost and
  has not been attempted. The gap is worth stating precisely: the current form has no horizon integral
  and therefore no ground truth to be measured against.
- **Every number in `## State` is `[SET]`.** The radius in particular is an eye-height judgement; nothing
  measures what radius the frame actually needs at other altitudes, and the pass has no altitude
  behaviour at all.
- **No AO acceptance measurement exists.** The obvious one is the same differential
  [`../visual-target.md`](../visual-target.md) §1.3 specifies for the whole renderer: the same scene path
  traced offline versus the engine, with the AO term on and off, and the distance between them.
- **Half-resolution upsampling has no edge test.** A bilinear upsample across a depth discontinuity
  bleeds occlusion across silhouettes; nothing measures whether it does so visibly at 720p.
- **Nothing stops a fragment entry point from returning a diagnostic.** Twice now a stage has shipped
  a debug expression in the one channel a consumer reads, and both times it survived weeks because the
  picture stayed *plausible*. A shader whose output is never compared against a computed expectation
  has no gate at all; the tone-probe ruler used above is the cheapest one that exists and it is run by
  hand.
- **The occlusion floor and the ambient split now interact visibly.** With the AO live, a roof cap deep
  in a courtyard reaches `mix(kAoFloor, 1, 0.12)` = 0.34 and lands at display code 8. That is not
  black and it is not measured against anything — no ground truth says whether 0.25 is the right
  floor.

## Knowledge

Nothing is derived here yet. The one sourced number is GTAO's published cost in `## Spec`; everything the
built pass uses is `[SET]` and listed in `## State`. The scene-referred scale the AO term multiplies into
is derived in [`tonemap.md`](tonemap.md) `## Knowledge`.

### Sources

| # | Source |
|---|---|
| 13 | Jimenez et al., *Practical Real-Time Strategies for Accurate Indirect Occlusion* (GTAO), SIGGRAPH 2016 Course |

The numbering is [`../visual-target.md`](../visual-target.md)'s; a number means the same paper everywhere
in `doc/render/`.
