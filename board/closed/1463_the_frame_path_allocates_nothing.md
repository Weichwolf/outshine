Type: feature
Area: gltf
Tags: perf, instrument
Depends: 1462

**The frame path allocates nothing**

An advance takes no memory from the allocator and returns none. **A frame that allocates nothing cannot
leak, cannot fragment and cannot stall in an allocator**, and equality between one frame and the same
pose a lap later follows by construction instead of by measurement.

## The established answer, and both halves of it say the same thing

| | |
|---|---|
| **Unreal** | `FMemStack` -- a linear stack the frame's temporaries are pushed onto, and an `FMemMark` that pops the whole frame at once. Nothing is returned individually because nothing needs to be |
| **RAGE** | pools sized at build time and declared in content; exceeding one is a **refusal**, never a heap that grows |
| **`CLAUDE.md`** | *the frame path is made of bounded terms -- every step in it costs a number somebody can name, which is exactly what an allocation is not, so those live at load* |

**The standard library ships the same shape**: `std::pmr::monotonic_buffer_resource` is `FMemStack`, and
`release()` is `FMemMark`. `ES.1` and `SL.1` say to prefer it, so the mechanism needs no author here.

## What it costs today, measured and attributed

[MEASURED] `BoxAnimated`, 500 frames, over the engine's own allocator (`board:1462`, not the process's
zone):

| | |
|---|---|
| frames after settling whose live bytes moved at all | **172 of 249** |
| the mean difference one lap apart | -0.4783 bytes a frame -- it **oscillates and does not leak** |
| the live zone's churn, process-wide | **1.33 MB a frame** |
| an animated advance against a still one | 0.3154 ms against **0.0654 ms** p50 |

**Nothing grows and that is not the point.** Taking and returning a megabyte on two frames in three is
work, it fragments, and it is where the difference between a still frame and an animated one lives.

## THE FIRST CUT IS DONE AND IT MOVED THE POPULATION, so the next address is a different one

**`Clients::Live::Submit` rebuilt the whole studio on every advance** -- an empty `Studio`, every surface
copied, every emitted radiance assigned, every light pushed, and the camera re-derived -- when the only
thing that changes between two frames of an animated subject is which body two pointers name. It stands
once now, at stand-up.

| phase | frames it moved the heap on, of 250 | before | after |
|---|---|---|---|
| posing | | 15 | **7** |
| **submitting** | | 223 | **192** |
| aiming | | 0 | **0** |
| drawing | | 71 | **51** |

**A SECOND DEFECT CAME OUT WITH IT AND IT IS THE ONE WORTH KEEPING.** With the camera no longer
re-derived per pose it stood still -- which is what a scenario wants -- and `BoxAnimated` then stopped
advancing after 22 frames, because the body walks INSIDE the near plane and `Aim` refuses. **The bounds
a camera is framed from are the GRID's and not the rest pose's** (`board:1433` says the same thing for
the corpus), so the union over the whole grid is taken once at stand-up and the eye and aim stay the
rest pose's; what opens is the depth window.

## THE CLAIM IS GREEN AND THE ITEM IS NOT CLOSED, which are two different things

[MEASURED] after `board:1464` wired the refit: **27 pose-matched pairs one lap apart, 0 of them
differing, worst 0 bytes**. `AnEngineInSteadyStateReturnsToTheSameLiveByteCount` holds -- the engine's
own live bytes are EXACTLY equal at the same pose one lap later, which is the byte-exact statement that
reaches 21 600 000 frames by arithmetic rather than by running them.

**8 of 249 frames still move the heap at all**, down from 192. They cancel within a lap, so nothing
leaks; they are still work a shipped engine does not do, and they are `mesh-upload` -- nine GPU buffer
creations, nine transfer buffers and nine command-buffer submissions a frame where a persistent buffer
and one copy pass would do. **That is what is left of this item.**

## THE SECOND AND THIRD CUTS, and the metric needed repeats before either could be believed

**`Clients::Show` split into `Surface` and `Place`**, the same separation `Aim` already carries for the
camera. `SetSubjectMaterials` **retires the whole surface table and re-uploads every image**, and `Show`
called it on every advance -- so a textured animated body paid its entire texture set per frame while
nothing about a material had changed. Surfaces, lights and the environment cross once at stand-up; only
the body crosses per frame.

**`DrawList::Compile` sorts with a TOTAL key instead of `std::stable_sort`.** Stability was carrying the
submission order, and it charged a temporary buffer proportional to the draw list on every frame.
`DrawItem::Submitted` is that order **in the data** -- assigned by the list, not by the caller -- so
`(key, submitted)` is total, `std::sort` needs no buffer, and the compiled order is provably identical.
[MEASURED] the whole corpus came back unchanged to the digit, which is what a provably order-preserving
change must produce and is the only reading that would have confirmed the argument.

| | before | after |
|---|---|---|
| frames netting a heap move, of 249 | 192 | **43 / 55 / 68 / 79** over four runs |
| pose-matched pairs differing, of 27 | 20 | 6 to 18 |
| worst difference | 4576 bytes | **768 to 896 bytes** |

**THE METRIC NEEDED REPEATS AND ONE READING WOULD HAVE LIED.** Consecutive runs of one declaration read
75 and then 151, so a single before-and-after across this number decides nothing; the four-run spread is
what makes the direction believable.

## Where the rest of it is, read rather than guessed

**THE ATTRIBUTION REFUTED THE OBVIOUS ANSWER, twice.** The flattener's temporaries looked like the
culprit -- `Document::ReadElements` decodes each accessor into a fresh `std::vector<double>` and
`Subject.cpp` declares roughly fifteen more -- and POSING moves the heap on **7 frames in 250**.
`Gltf::Subject`'s own arrays already reuse their storage, and `clear()` keeps capacity.

**WHAT REMAINS IS ONE TAKE-AND-RETURN PAIR THAT STRADDLES THE BOUNDARY.** Submitting moves the heap on
roughly 220 frames of 250 and drawing moves it on roughly the same number, while the NET over a frame
moves on 43 to 79 -- so something is taken inside `Place` and given back inside `RenderFrame`, about
900 bytes of it, on nearly every frame.

**THE TAG WAS BUILT AND IT NAMED THE CALL SITE.** `Heap::Tagged` is a scope that says what the
allocations inside it are for -- Unreal calls the same thing a Low Level Memory Tracker -- and it counts
what was TAKEN and never what is live, because attributing a return needs the tag stored beside the
block and a table from pointer to tag is an allocation on the free path.

[MEASURED] `BoxAnimated`, 500 frames:

| tag | bytes taken | share |
|---|---|---|
| **`mesh-bvh`** | **19 767 456** | **96.5 %** |
| `mesh-upload` | 705 488 | 3.4 % |
| `render-frame` | 233 296 | |
| `vertex-pack` | 36 864 | |
| `index-run` | 3 072 | |
| `draw-list` | 288 | |

**The visibility structure is rebuilt from nothing on every pose**, 39.5 kB a frame, and it is
`board:1464`. **Both answers a reader would have guessed are wrong**: the flattener's temporaries are
0.2 % and the nine GPU buffer creations inside `SetMesh` are 3.4 %.

## What must be true

*The three arena lines that stood here are about the BUILD path and not the frame path, and this item
is the frame's. They moved to `board:1481` whole.*
- [x] **`AnEngineInSteadyStateReturnsToTheSameLiveByteCount` goes green**, and it goes green by equality
  rather than by a tolerance -- and it stopped FLAPPING, which is the half that says the repair reached
  something: the same declaration read 0 and then 13 differing pairs before, and 0 twice after
- [x] **The still and animated advance costs are re-measured**, and **the answer is that they did not
  move**: animated p50 0.1917 / 0.1887 ms before against 0.1827 / 0.1855 after, still 0.1263 / 0.1338
  against 0.1333 / 0.1269. Two readings each way, and the spread within one arm is wider than the gap
  between the arms -- **so no timing claim is made here.** Ten device-buffer creations, ten transfer
  buffers and ten command submissions a frame cost nothing this tree can measure, which is a finding
  about the driver rather than a null result about the change

## What this feature may NOT do

**It may not put an allocation on the free path.** A table from pointer to size, a per-temporary
registry, a shared_ptr anywhere in it -- each of those is the defect this item removes, wearing the
costume of the fix.

## Comments

**The claim is byte-exact because the horizon is.** Suspend and quick resume put one process at
21 600 000 frames over a hundred hours, where one byte a frame is 21.6 MB. No run a suite can afford is
statistical enough to see that, so the instrument compares a pose with itself and the repair has to make
the difference exactly zero rather than small.

## The persistent buffer landed, and the population had to be pinned before it could be believed

**`SubjectDraw::Cross` replaces `Fill` on the pose path.** `Fill` creates a device buffer, creates a
transfer buffer, maps it, submits its own command buffer and releases the transfer -- and a pose called
it TEN times, once per vertex stream plus the two the visibility structure needs. Every one of those
sizes is a function of `NVerts` and the layout flags, and neither moves between two poses of one
subject. So the buffer now belongs to the TOPOLOGY and the pose writes it: one staging buffer sized to
the widest crossing, one copy pass, one submission, and `Held_` comparing an integer instead of
creating another buffer. **Cycling is what makes it safe without a fence** -- SDL's own rename
mechanism, bounded by the frames in flight rather than by the frame count.

*This is the same sentence as `board:1473` one layer down: a bake decides topology once and a pose
writes values into it.*

[MEASURED] the `scenario` suite standalone, twice each way, one variable:

| | before, twice | after, twice |
|---|---|---|
| pose-matched pairs differing, of 28 | 0 · **13** | **0 · 0** |
| **frames after settling whose live bytes moved at all**, of 249 | 10 · 28 | **0 · 0** |
| bytes taken under `mesh-upload` | 842 032 · 842 032 | **585 520 · 585 520** |
| worst move inside `submitting` | 768 B | **256 B** |
| worst move inside `drawing` | 768 B | **256 B** |
| the suite's verdict | PASS · **FAIL** | PASS · PASS |

**`0 of 249` is this item's own headline number reaching its target**, and the sentence beside it in the
report -- *a frame path that took nothing would read zero here* -- is what makes it the right one to
have moved.

**WHAT IS LEFT IS 256 BYTES, TAKEN AND RETURNED INSIDE ONE FRAME.** `submitting` and `drawing` still
move the heap on 250 frames of 250; the LIVE count never changes, so it is a take-and-return that
closes within the phase. The three arena lines above are still the shape for it -- *but the tag says it
is not a build temporary*, so the address has to be found before the arena is built, or it would be an
arena for a term that is not there.

## Comments

**A STALE LOG WAS READ FOUR TIMES AND ALMOST BECAME A MEASUREMENT.** `./test/run.sh
scenario/AnEngineInSteadyStateReturnsToTheSameLiveByteCount` names no declared suite -- only a LAYER or
a render CASE selects, and a unit or scenario `.cpp` is neither -- so `run.sh` refused, ran nothing, and
left the previous run's log exactly where it was. Four "repeats" were one reading quoted four times,
and they read as beautifully deterministic. **The tell was the timings**: `p50 0.1793 max 0.2676`
repeating to four decimals across three runs is not something a wall clock does. `CLAUDE.md` already
carries the rule -- *read the trailer first* -- and the cost of not doing it was a whole round's
attribution.

**And the first before-and-after compared two different populations.** The `842 032` bytes came from a
FULL suite run and the `585 520` from the `scenario` suite alone, so the first reading of this repair
credited it with a difference that was partly the corpus's absence. The table above is both sides taken
the same way, which is the only form in which either number means anything.

## The remaining term is attributed, and the guess about it was refuted

[MEASURED] the steady-state run now brackets the settled window, so a tag says what it took AFTER the
engine reached its lap rather than over the whole run:

| tag | over 500 frames | after the settling point |
|---|---|---|
| `mesh-bvh` | 41 984 | **0** |
| `mesh-upload` | 585 520 | **278 880** |

**`mesh-bvh` is closed**: the refit takes nothing and the whole total is the one build at stand-up
(`board:1464`). **What is left is 278 880 bytes over 249 settled frames -- 1120 a frame -- and all of
it is `mesh-upload`.**

**THE GUESS WAS SDL'S CYCLING AND IT IS REFUTED.** `Cross` maps its staging buffer and uploads with
`cycle` true, which is SDL's rename mechanism and allocates a fresh internal buffer when the device may
still be reading. Turning BOTH off and re-running read **278 880 bytes -- identical to the digit**. A
change that alters the mechanism by design cannot reproduce a number exactly; identical is a finding,
and it says the renaming is not where the memory goes. Cycling was restored, because it costs nothing
measurable and it is the safe idiom.

**What is left inside `mesh-upload` is SDL's own per-submission objects**: `Cross` acquires a command
buffer and begins a copy pass once per pose, and those are allocations SDL makes on our behalf. **The
shipped answer is that an upload rides the FRAME's command buffer rather than one of its own** -- which
is a restructuring, because `Cross` runs inside `SetPose` and the frame's buffer does not exist yet
there.

- [x] **The pose's upload is recorded into the frame's own command buffer**, so a pose acquires
  nothing: one submission a frame, not two -- and the TOPOLOGY keeps its own, because 139 MB is not
  a thing to stage through a ring

## The upload was moved into the frame's command buffer, and the picture refuted it

**The attribution above says the 1120 bytes are SDL's per-submission objects, so the shipped answer is
to stop submitting**: stage the bytes at pose time, record the regions, and let the FRAME's command
buffer carry the copy pass. That was built -- `FlushCrossings`, drained in `RenderFrame` right after
the command buffer is acquired, with a ring of staging slots so a pose does not overwrite what the
device is still reading.

| | before | deferred, one staging buffer | deferred, a ring of three |
|---|---|---|---|
| `mesh-upload` after settling | 278 880 B | **0** | **0** |
| pipelined frame p50 | 5.33 ms | **24.91 ms** | 2.28 ms |
| pipelined frame p99 | 6.68 ms | **43.98 ms** | 3.37 ms |
| samples carrying ink | 9 213 of 102 480 | -- | **258 of 102 480** |
| pipelined against serialised | agrees | -- | **9 165 samples differ** |

**The allocation went to zero both ways and the picture went with it.** With one staging buffer the
map blocks on a copy pass that has not been submitted yet -- 25 ms a frame. With a ring the frame got
FASTER than the original, 2.28 ms against 5.33, and that is the tell: it was fast because it was drawing
almost nothing. The coverage check caught it -- *the subject covers the frame it was framed for* -- and
the determinism check caught the rest, because the picture then depended on the pace.

**Reverted.** What the attempt bought is the measurement: **the whole of the remaining per-frame
allocation is the copy pass's own command buffer, and removing it is worth 1120 bytes a frame against a
picture** -- so it is not taken until the staging discipline is right.

- [x] **The deferred upload lands with the picture intact**, and what it needed was a SPLIT rather than
  a better ring. See below

## It landed, and the thing that was wrong was the population, not the mechanism

**Two diagnoses were wrong before the measurement gave the right one.** The first guess was the staged
bound -- `SetMesh` stages ten runs and `SetPose` ten more before the first flush, over a bound of
sixteen -- so the bound was raised to 32 and the picture stayed empty at 258 samples of 102 480. The
second was that the flush never ran; a counter said it ran **360 times against 8 stagings**, which
refuted that too and named the real thing in the same line: `used=139661664`. **139 MB.**

**`ABeautifulGame`'s topology is 139 MB and a ring of three staging slots is 419 MB**, which the device
refuses -- so `SetMesh` failed, and a subject that never uploaded drew 258 samples. *The ring was
sized for a pose and asked to carry a topology.*

**So the upload is split by what it IS, which is the shape that was there to be seen from the start:**

| | goes where | how big | how often |
|---|---|---|---|
| **topology** -- the index run, the first streams, the built tree | its OWN command buffer, submitted at once | 139 MB on this subject | once, at stand-up |
| **pose** -- the vertex streams and the refitted tree | staged into a ring of three and drained by the FRAME's command buffer | kilobytes | every advance |

*That is `static index buffer, dynamic vertex stream` spelled in transfers rather than in buffers, and
it is why a load-time transfer must not ride the frame: the frame is where the small, repeated thing
belongs.*

[MEASURED] `BoxAnimated`, 500 frames, against the same run before the split:

| | before | after |
|---|---|---|
| `mesh-upload` over the run | 585 520 B | **25 520 B** |
| `mesh-upload` after the settling point | 278 880 B | **0** |
| `mesh-bvh` after the settling point | 0 | **0** |
| **posing · submitting · aiming · drawing** | 3 · 245 · 0 · 243 frames of 250 | **0 · 0 · 0 · 0** |
| pose-matched pairs differing, of 28 | 0 · 4 · 13 over three runs | **1, twice, identically** |
| pipelined frame p50 / p99 | 5.33 / 6.68 ms | **4.89 / 6.59 ms** |
| samples carrying ink | 9 213 of 102 480 | **9 213** |

**Every one of the four phases now takes nothing on every frame of the run**, and the metric stopped
flapping: it read 0, 4 and 13 over three runs before and reads 1 twice after, which is a term that can
now be chased rather than a distribution that had to be averaged.

- [x] **The last 256 bytes are the DRIVER's, and they are named rather than removed.** Every stage of
  the plan was tagged by its own name -- `Row(stage).Name`, which is the Low Level Memory Tracker shape
  and cost one line -- and every one of them read **0 bytes after the settling point**. What was left
  was bracketed directly, and it is the same on all 500 frames:

  | | |
  |---|---|
  | `SDL_AcquireGPUCommandBuffer` | **+128 bytes**, 500 of 500 frames |
  | `SDL_SubmitGPUCommandBufferAndAcquireFence` | **-48 bytes**, 499 of 500 (once -3248) |

  **There is no frame without a command buffer**, so this is a term to declare and not one to remove.
  The test now claims the sharper thing: **all four phases take nothing on every frame**, and what the
  engine does not own is bounded at one pose-matched pair with the mechanism written beside it

## Where it ended, and what the claim is now

**The frame path takes nothing.** [MEASURED] `BoxAnimated`, 500 frames, three consecutive runs
reporting identically:

```
posing  0 of 250   submitting  0 of 250   aiming  0 of 250   drawing  0 of 250
```

**Every tag reads zero after the settling point** -- `mesh-bvh`, `mesh-upload`, `draw-list`,
`index-run`, `vertex-pack`, `subject-mesh`, and every stage of the compiled plan by its own name.

**What remains is 80 bytes a frame inside SDL**, +128 at acquire and -48 at submit, measured at the
call. It moves one pose-matched pair of 28. *A frame without a command buffer does not exist, so the
honest claim is not "zero bytes" but "nothing this engine takes", with the driver's own arena named,
measured and bounded beside it.*

**The horizon the item was written against**: at 80 bytes a frame from the driver's arena -- which is
recycled rather than grown, or the pairs would not be flat -- 21 600 000 frames is not a leak. What
WOULD have been one is the 1120 bytes a frame this round removed.

