Type: bug
State: open
Area: render
Tags: shadow, plan, residency

# A plan that drops the shadow stage clears the atlas rather than leaving the last one standing

**Benchmark** — Unreal: RDG will not let a pass read a resource no pass wrote in this graph — the dependency is declared and the compiler of the graph enforces it. RAGE: fixed passes, so the case cannot arise. **Taking Unreal** — a stage that leaves is a dependency that leaves with it.

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

**Why the arm that would prove the flag cannot be built.** The first attempt was a re-declaration
carrying no asset: it kept the previous subject standing, which board:1927 owns and which is now
repaired -- the door case proves it, `DECLARING NOTHING draws 0 batch(es)`. But an EMPTY stage
proves nothing about the flag: with no batches the subject stage never encodes, so the count
cannot grow whether the flag is cleared or not. Measured: with `CastsNoShadow()` removed the case
still passes.

The arm that would prove it needs a subject that DRAWS and does not CAST, and the declaration
language cannot say that -- `Lit.ShadowRadiusM` only turns shadows on, and a subject with no
extent is refused before it stands (correctly: no camera can be derived from it). board:1928 owns
it. Until then the flag repair stands on construction alone and this item says so.

Proving case:  an engine that renders with a caster and then re-renders a declaration carrying
none reads an atlas with no texel above the clear. Negative control: the plan as it stands, and
the second read repeats the first read's count exactly.
