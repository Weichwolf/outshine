Type: feature
Area: render
Tags: perf, instrument

**The temporal resolve the plan already declares is executed**

Every pipe is laid and the stage is not built. `RenderCatalogue.h` carries the row --
`temporalResolve`, reading `SceneComposited`, `SceneVelocity`, `SceneDepth` and writing `SceneLinear`
-- `RenderPlan` adds `kTemporalSettleFrames` when it is held, `SceneLinear` **`FallsBackTo`**
`SceneHdr` when it is not, and the velocity target is written by every subject arm already.

**And `Renderer.cpp` answers it in three places with `this device layer does not execute the stage`,
while `jitter` appears nowhere in `src/` outside the vegetation tables.** So the declaration is real
and the picture it describes has never been made.

## Why it comes before the visibility buffer, and it is not a preference

| | this | `board:1412` |
|---|---|---|
| provenance | **`Content`** -- a case declines it and the alias carries the picture | machinery under the subject unit |
| effect on the 148 corpus cases | **none**, because they declare no temporal stage | every case re-measured |
| gain at 720p on this device | large and immediately visible | small until triangles approach pixel size |
| what already exists | velocity, settle frames, the alias, the stage row | nothing |

**And there is an ordering dependency in the other direction**: `board:1412`'s resolve recomputes
barycentrics from PROJECTED triangle corners, and a temporal jitter moves the projection by a sub-pixel
offset every frame. Built the other way round, the visibility buffer would have to take the jitter in
afterwards.

**At a third of a PS4's bandwidth and more compute, this is the shape the device asks for**: it buys
picture quality with arithmetic and a history read rather than with resolution -- and it is the
precondition for a dynamic resolution that holds a hard frame floor, which is the id Tech 7 line
`CLAUDE.md` already cites.

## What must be true

- [ ] **The jitter is declared and deterministic**, a named sequence with a named length and amplitude
  -- *the mathematics is deterministic*, so two runs of one scenario produce one picture
- [ ] **The history belongs to a frame and is NOT a plan edge.** A read of the previous frame's output
  is a cycle in a per-frame DAG; the renderer owns the history texture across frames and
  `SettleFrames_` already exists for exactly this
- [ ] **The velocity the reprojection uses is the one the geometry pass writes**, so the two cannot
  disagree about what moved
- [ ] **The neighbourhood clamp is stated once** and its colour space is named -- history that is not
  clipped towards the current frame is ghosting, and clipping in the wrong space is a colour shift
- [ ] **A plan that does not declare it pays nothing**: no blit, no history allocation, no settle
  frames. The alias already promises this and the promise is checked
- [ ] **The frame cost is published before and after** by the suite that already runs four arms over a
  moving camera
- [ ] **The corpus does not move.** No case declares this stage, so a case that changes is a case that
  was reading something it should not have

## What it will NOT do in this round

**No dynamic resolution.** That is what this unlocks and it is a separate item with its own
measurement; landing both at once would leave neither attributable.

## The catalogue already decided the shape, and a separate stage is REFUTED by it

**`TemporalResolve` FUSES INTO `Tonemap`.** Its row's last field is `Stage::Tonemap` where every other
stage carries `kNoFusion`, `RenderPlan` sets `Fused_[Tonemap]` and puts both into ONE pass, and it
refuses a plan that resolves without requesting a picture in words that say why: *the resolve and the
display transfer are one fragment, so a plan that resolves must also request a picture*.

**A separate `TemporalResolveStage` was built first and the measurement killed it.** [MEASURED] with the
resolve as its own stage, all three frame arms produced **byte-identical** pictures --
`dc59986575fe` for `fill`, `fill-twice-lit` and `texture` alike -- because the resolve drew its
full-screen triangle into the TONEMAP's pass and target, and the tonemap then drew over it while
sampling a `SceneLinear` nothing had written.

**The fused shape is also the better one on this device**, which is why the catalogue is right rather
than merely prior: one fragment writes **two** targets -- `SceneLinear`, which is next frame's history,
and `FrameTex`, which is the display -- so the resolved scene is never written, read back and written
again. At 720p60 that is one round trip of 8 B/px, **442 MB/s each way**, saved by construction.

## What that leaves to build, and the arithmetic already exists

- [ ] **The resolve moves into the generated display fragment** (`Resolve.h`), under the same
  declaration-driven splice the transfer already uses: a plan without a temporal stage emits no
  temporal term at all rather than a term behind a flag
- [ ] **It writes two colour targets** and the pass attaches both, which is what fusion means here
- [ ] **`SceneLinear` stays double-buffered** and the swap stays where it is

**What was built and is correct and transfers whole**: the YCoCg neighbourhood clip towards the mean,
the velocity with the jitter difference taken back out, the Halton(2,3) sequence in the projection's z
column, and `BeginTemporalRun`.

## Three defects found on the way, all of them real and two of them the engine's

**A SHARED CASE BODY MADE A HELD RESOURCE ALLOCATE AT THE WRONG FORMAT.** `SceneLinear` was added as a
label on the arm that returns nothing, so `Meter`, `ShadowAtlas` and `AoBuffer` fell into it -- and the
tonemap reads `AoBuffer`, so it is held on every plan. Metal aborted with *MTLTextureDescriptor has
invalid pixelFormat (0)*.

**THE VELOCITY REFUSAL WAS TOO STRICT AND SAID SO IN ITS OWN WORDS.** It refused a velocity target over
a mesh with no previous pose because *the motion of every pixel would be a sentinel* -- and that is not
what happens. A screen-space motion vector has two terms, and a mesh with no previous pose is a mesh
that DID NOT DEFORM: its previous position is its current one and the camera term is untouched. The
rule kept a temporal resolve off every static subject there is. **Repaired: a rigid mesh is its own
previous pose, so the vertex term is zero by arithmetic rather than by a branch.**

**A TEMPORAL RUN HAS A BEGINNING AND NOTHING DECLARED ONE.** History carried across a camera cut is the
previous view bleeding into this one, and the frame suite's own *every repeat of an arm drew the same*
caught it immediately -- the second repeat began with the first one's accumulation. `BeginTemporalRun`
is now the consumer's statement that a sequence starts here, and it is NOT detected from the camera: a
threshold on how far the eye moved would be wrong in both directions.

## The fused resolve is built and it is NOT declared, and that is a state with one open question

**It works.** With the resolve folded into the display fragment the arms stopped drawing one picture:
`the textured arm's images reached the sampler` went green, which is the check that says the resolve is
producing a per-subject picture rather than overwriting one.

**And then the frame suite refused it, correctly.** [MEASURED] *every repeat of an arm drew the same
picture* is red on **three arms of four -- `fill`, `fill-twice-lit` and `texture`, every one of which
carries a light -- while `geometry`, the one with `lights=0`, holds.** The repeats compare
`MedianCoveredPx` and `SumRadiance` from `WhatIsDrawn`, which begins a temporal run per probe and so
renders exactly one frame with no history at all.

**So the picture is a function of something other than the declaration, and that is the one thing this
engine's own rules forbid outright.** Two readings, and neither is established:

- [ ] **The light path.** The three that fail carry one, the one that holds does not. A jittered
  projection moves each fragment's world position by a sub-pixel, so a shadow ray near a silhouette can
  flip -- but that is deterministic for a fixed jitter, and the jitter is reset per probe
- [ ] **The coverage.** The three that fail also cover 542 207 px against `geometry`'s 32 531, so a
  single differing pixel is far easier to see in their sum. **If that is it, all four are affected and
  only one is sensitive enough to say so** -- which would make this a finding about the instrument's
  reach rather than about the light

**The declaration is withdrawn until that is answered**, so the suite is green and the resolve reaches
no picture. *That leaves code nothing runs for one round, which is named here rather than hidden -- and
it is the lesser of the two: a red instrument nobody trusts is worse than a stage nobody has switched
on.*

## What IS committed and stands on its own

- **The fused fragment**: one pass, two targets, `SceneLinear` for the next frame's history and
  `FrameTex` for the display, so the resolved scene is never round-tripped
- **The YCoCg neighbourhood clip towards the mean**, with the sources it comes from
- **Halton(2, 3) in the projection's z column**, which is what makes it anti-aliasing rather than a
  smoother of one set of samples, and zero on every plan that declares no resolve
- **`BeginTemporalRun`**, the consumer's statement that a sequence begins -- never a heuristic on the
  camera
- **The rigid-mesh velocity repair**, which stands entirely on its own and unblocked this

## CORRECTION -- there was never a nondeterminism, and the two readings above were both wrong

Both candidates this item named are refuted, and by the discriminator it asked for and by one it did not.

**THE DISCRIMINATOR IT ASKED FOR, BUILT AND RUN.** A fifth arm, `fill-unlit`: `fill`'s subject at
`fill`'s standpoint with **no light**. [MEASURED] it covers **542 207 px, exactly what `fill` covers**,
and it holds. So the coverage reading is dead -- a picture that big is not too big to compare.

**THE ONE IT DID NOT ASK FOR.** The repeat check compared `MedianCoveredPx` and `SumRadiance` and said
only *a repeat drew something else*. Made to say **which and by how much**, it answered:

```
DIFFERS fill repeat 1: covered 542207 against 542207 (+0 px), radiance nan against nan
```

**The coverage is identical to the pixel at every repeat and the sums are NaN**, so the inequality was
`nan != nan` and nothing else. *The resolve is deterministic. It has always been deterministic.* What it
is not is finite.

## What is actually wrong, with its size

[MEASURED] over the probes of one run, covered pixels carrying a non-finite radiance:

| arm | lights | non-finite covered px | first at |
|---|---|---|---|
| `fill` | 1 | **924 488** | index 336 |
| `fill-twice-lit` | 2 | **2 074 846** | index 336 |
| `texture` | 1 | **260 121** | index 451 |
| `geometry`, `fill-unlit` | 0 | **0** | -- |

**Identical at every repeat**, which is the same statement as above from the other side.

**And a NaN in this fragment spreads by construction.** One non-finite pixel enters the next frame's
history; the 3x3 box around it is then non-finite, so `centre` and `extent` are NaN, `largest` is NaN,
`largest > 1.0` is false, `clipTowards` returns the history unchanged, and `mix` carries it out. That is
how one seed becomes 43 % of a picture. **The seed is the open question and the light path is where it
is**, which is the one thing the original reading got right.

## What was repaired on the way and stands on its own

**The neighbourhood wrapped at the target's edge.** `scene.read(uint2(int2(px) + int2(dx, dy)))` asks for
`uint(-1)` -- 4 294 967 295 -- on the first row and column, and a texture read out of range is undefined
in MSL. It is clamped to the target now. **[MEASURED] it is NOT the seed**: the counts above are from the
run after the clamp.

## Two instruments got stronger and they are what will close this

- **`WhatIsDrawn` counts non-finite covered pixels and names the first**, and the sum skips them so the
  repeat comparison measures the picture rather than measuring NaN
- **A covered pixel carrying a finite radiance is a CHECK**, not a note -- because the sum skipping a
  term nobody counted would be a green resting on exactly what it stepped over
- **`fill-unlit` stays in the arm table.** It cost one arm's time and it retired a reading that had stood
  since this item was opened

## Four candidates went into the probe and one came out, and it is not shading at all

Each of these ran with the stage declared, one change at a time, and every count below is over the same
probes as the table above.

| probe | result |
|---|---|
| **coverage** -- `fill-unlit`, `fill`'s subject and standpoint with no light, **542 207 px** | 0 non-finite. Refuted |
| **the history and the clip** -- the `inside && historyHeld` branch disabled outright, so `kept = here.rgb` | **identical counts**. Refuted |
| **the jitter** -- the stage declared and `Jitter_` forced to zero | **identical counts**. Refuted |
| **the fragment's own write** -- `out.linear = float4(7, 8, 9, 1)` | **the readback still shows `(nan, 0, 0, 0)`** |

**THE LAST ROW IS THE FINDING.** The fused fragment writes a constant into `SceneLinear` and
`ReadSceneLinear` does not see it, so **the resolve's second attachment and the texture the readback
resolves are not the same thing** -- and everything above it was a search for a shading defect that was
never there.

*A first pass at the history probe was invalid and is recorded because it cost a round: setting
`kCurrentWeight` to 1.0 does NOT bypass the clip, because `mix(a, b, 1)` is evaluated as `a + (b - a)*t`
and a NaN in `a` survives it. The branch has to be disabled, not weighted away.*

**What the non-finite pixel looks like is consistent with a target nobody wrote**: `(nan, 0, 0, 0)` at a
pixel whose depth is 0.311709 -- three channels at the clear value and one holding whatever was in the
allocation. **And it is not simply uninitialised memory either**: `fill` and `fill-unlit` cover the
*same* 542 207 pixels of the *same* textures in one process, and one reports 924 488 non-finite while the
other reports none. Whatever writes that texture, the lighting reaches it.

## Where the next round starts

**`Target(Plan_->Bound(Resource::SceneLinear))` against the attachment the fused pass actually binds.**
One is the ping-pong pair `LinearTex_[LinearAt_]`, the other is whatever the pass built its attachment
list from, and the constant probe says they differ. That is a question about the plan's attachment
construction and not about a BRDF.
