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
