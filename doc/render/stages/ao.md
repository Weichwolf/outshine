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

**Built as a sampled-disc SSAO, uncommitted.** `AoStage.{h,cpp}` exists in the working tree and is
reported untracked by `git status` at the time of this split; the round that builds it is running
concurrently, and **its commit anchor and frame measurement are owed by it**.

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
