Type: issue
State: open
Area: render
Tags: architecture, stage, measured

# The subject stage does one thing, and the other five move out

**Benchmark** — Unreal: one pass does one thing and RDG makes the seams explicit. RAGE: fixed passes with clear jobs. **Both agree** — a stage that does six things has no dependency a graph can read.

`SubjectDraw` is the largest red on both CURRENT maps. Six responsibilities in one class,
beside the one `Encode` a stage owes:

## Measured 2026-08-25: the list over-counted and the line count is the real signal

`ShaderSource` is NOT one of SubjectDraw's extra responsibilities. Six of the twelve stages carry
one -- OverlayDraw, SkyStage, TonemapStage, PresentStage, CompositeTransmissionStage and this one
-- so a stage owning its shader source is the house style, and an item that counts it as a defect
counts the house.

What the census does say, over every stage:

| stage | lines | Pipeline | Encode | Set* |
|---|---|---|---|---|
| SubjectDraw | **1126** | **5** | **2** | **5** |
| SubjectResidency | 391 | 0 | 0 | 0 |
| ParticipatingMedium | 350 | 0 | 0 | 0 |
| OverlayDraw | 278 | 1 | 2 | 2 |
| every other stage | 115-171 | 1 | 1 | 0-2 |

Eight times the median stage, three times the largest other one, five pipelines where every other
drawing stage has one, five setters where the next has two, two encodes where every other has
one. The size IS the finding and the list was the wrong instrument for it.

## The second encode is gone

`EncodeDepthOnly` was the shadow pass calling back into the colour stage:
`LightVisibilityStage::Encode` did nothing but `Subjects_->EncodeDepthOnly(...)`. The depth-only
source, its pipeline and its encode are the SHADOW stage's own now -- 124 lines moved --
and `SubjectDraw` reads back through four narrow const accessors instead of owning the pass.
1126 lines -> 1006.

And moving it found what it was hiding: **the shadow cast NOTHING.** `Live::Declaration::
ShadowRadiusM` was declared, read in two places, and written by nobody -- so it was always zero,
the plan never declared a shadow stage, and `SetShadowFrame` was never called. Measured through
the door: 258 batches drawn, 0 cast. A scenario can declare `shadowRadiusM` on its `<lighting>`
now, and one that does not gets a radius derived from the subject's own extent: 258 drawn, 258
cast.

Proving test: `outshine/door/ScoreWhatTheShadowCasts` -- the picture draws 1 batch and
the shadow casts 1, with no radius declared. Negative control: the derivation removed, and the
shadow casts 0 against 1 drawn.

| responsibility | at HEAD |
|---|---|
| shader source | `ShaderSource(const SourceOptions &options)` — SubjectDraw.h:30 |
| pipeline table | `PipelineAt(VertexLayout layout, SurfaceKind kind, bool cullsBack)` — :154 |
| upload / staging | `FlushCrossings(SDL_GPUCommandBuffer *commands)` — :148 |
| placements | `SetPlacements(const double *models, size_t rows, std::string &error)` — :51 |
| lights | `SetLights(std::span<const SubjectLight> lights, std::string &error)` — :89 |
| a second encode | `EncodeDepthOnly(const double lightFromWorld16[16], ...)` — :96 |
| **the stage's own** | `void Encode(const FrameContext &ctx, const PassRecording &into)` — :93 |

`SubjectResidency` already stands green beside it, which proves the split is available and
was only ever half taken.

## Measured again at b0b59b3a, and two of the four boxes are wrong

`SubjectDraw` is **816 lines** now, from 1126: `EncodeDepthOnly` left with board:1575's shadow
stage and took 124 lines and the second encode with it. `SubjectDraw.h` is 190 lines with a
surface a reader can hold.

**The five pipelines are not a defect, and the benchmark says so.** `Configure` builds
`kVertexLayoutCount * 2 * kSurfaceKinds` up front (SubjectDraw.cpp:324). Building a pipeline
when a draw first needs it is precisely the PSO hitch Unreal has spent years apologising for;
RAGE's answer is the same as this one -- pay at stand-up, decide nothing at the hot path. An
item that counts them as a responsibility counts the house style twice, the way the `ShaderSource`
row already did before the last correction.

**The transmissive box is refuted** by board:1909: `subjectsTransmissive` reads `SceneHdr` and
writes `SceneTransmissive`, so it cannot be a batch partition of the pass that produces what it
reads.

**The unit-twin box names a directory that no longer exists** (`test/unit/render/stages/`) and a
test shape CLAUDE.md forbids -- a case that asserts the shape of our own architecture specifies
nothing while TARGET moves.

## What is left, and it has no negative control

An extraction is closed by "the picture is unchanged", and no defect can be restored to make that
red. So this item cannot be closed the way every other item here is closed, and saying so is
better than performing a refactor and calling it a repair. What would close it is a DEFECT found
inside the split -- a field carried and not read, a second spelling, a decision at the hot path --
and none has been found by two measurements.

## What will be true

- [ ] The stage encodes and nothing else: source, pipelines, residency and the light table are
      their own units, each reachable by a unit twin that does not stand up a device.
- [ ] `EncodeDepthOnly` is the shadow stage's own encode (board:1575), not a second entry point
      on the colour stage.
- [ ] Transmissive draws are a batch partition of this one stage, so the cloned
      `subjectsTransmissive` row (RenderCatalogue.h:268) disappears (board:1574).
- [ ] Proving test: `test/unit/render/stages/` holds one twin per new unit; negative control —
      the encode handed an empty residency draws nothing and says so.
