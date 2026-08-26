Type: bug
State: open
Area: render
Tags: shadow, plan, residency

# A plan that drops the shadow stage clears the atlas rather than leaving the last one standing

`DeclarePlan` puts `Stage::LightVisibility` in the plan only when the declaration casts
shadows. When it does not, no pass is begun, so `ShadowAtlas_` keeps whatever the previous
declaration wrote. The next subject that reads it reads a caster that is no longer there.

Measured while closing board:1921: a scenario declared with one caster wrote 779 086 texels
above the clear; re-declared with no asset at all, the same read reported 779 086 again. The
light stage never ran, and nothing cleared what it had left.

The frame-graph question underneath: a resource a plan stops writing is either cleared or
declared stale, and today it is neither.

## What the measurement actually showed, and it corrects this item

The stale ATLAS is not the mechanism. Two things were found looking for it:

**The flag, and it is repaired.** `SubjectDraw::Shadowed_` was set true by
`Renderer::EncodeLightVisibility` and set false by NOTHING. A per-frame fact that only ever
becomes true outlives every frame after it. `Renderer::RenderFrame` now calls
`Subjects_.CastsNoShadow()` where it clears `Touched_`, so the flag cannot survive a frame and
is set only by the stage that ran. **This is unproven** -- see below.

**The undeclared dependency, and it is repaired.** `LightVisibility` handed the subject stage
`LutSamp_.Get()` as the atlas sampler, and that sampler is `Resource::LutSampler` -- pulled into
the plan by the MEDIUM stages and by nothing else. A declaration without a medium got a null
sampler, `Shadowed_` stayed false, and shadows switched off in silence. `Stage::Subjects` now
declares `Resource::LutSampler` among its reads, which is what makes the plan create it.
Measured: the door's caster arm went from 0 shadowed frames to 1.

**Why the arm that would prove the flag cannot be built yet.** The intended control was a
re-declaration carrying no asset, expecting no light stage. It does not work, and the reason is a
different defect: **a declaration naming no asset keeps the previous one standing.** Measured --
after `bare.Assets.clear()` and a re-`Declare`, `batches the picture draws` is still 1 and the
shadow radius is still the first arm's 0.7071 m. The engine is self-consistent there: it casts a
shadow for the subject that is actually standing. board:1927 owns that.

Proving case:  an engine that renders with a caster and then re-renders a declaration carrying
none reads an atlas with no texel above the clear. Negative control: the plan as it stands, and
the second read repeats the first read's count exactly.
