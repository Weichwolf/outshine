Type: feature
Area: clients
Tags: perf, instrument

**The fourth constraint has no instrument, no subject and no case — **and that is this round's finding, not a note****

*Recorded 2026-08-13, on the SDL_GPU port. `CLAUDE.md` names four constraints and says there are no
others: **SDL3 · SDL_GPU · modern C++ · this device at 720p60**. The port met the first three for the
first time. **The fourth is unmeasured, and every instrument that could measure it left the tree with
the renderer.*** `GpuTimer` was deleted for having no consumer — correctly, by § I.23's zero-consumer
rule — but the consumer was what was missing, not the instrument. **This is `not yet measured`, never
`not measurable`: it is effort, and the effort is named below.**

- [x] **The three things that must exist together, and TWO of them now exist.** `test/frame/TheVisibilityTermIsPricedPerRay` takes **p50/p95/p99 over a full orbit at 720p, 240 timed frames per arm** — so the **camera path** is declared, deterministic and moving, and the **per-frame clock** is beside it. *The instrument is no longer missing; the section's own sentence can now name which of the three is.* **It is the scene.** Nothing in the tree draws a landscape, so what the clock times is a subject on a card
- [x] **The frame test runs without a sanitiser and the reason is on the line rather than in a habit: a duration cannot be measured through a bounds checker.** *This is a declared deviation from the tree's default and it is the right one — an ASan build measures ASan. What it costs is that the timed arm is not the memory-checked arm, so the same declared run must exist twice or the timing is trusted code nothing checked; that is the next line, not this one*
- [ ] **The one that is still missing is the SUBJECT**, and it is the same absence § I.26.9's scenario suite has carried since it was defined: a declared world scene with terrain, vegetation and buildings, at 720p, over the orbit that already exists. **Until then the fourth constraint is priced per ray and not per frame** — 10.0 % of budget for one light on a 23 358-triangle subject is a real number about a real cost and is not a statement about 720p60
- [ ] **The suite it belongs to already exists and has no members: `scenario` (§ I.26.9).** Its placement rule already sends load time and frame time there, and `NodePerformanceTest` is already assigned to it. **A suite with a definition and zero cases is a claim, not an instrument** — and it has been that since § I.26.9 was written
- [ ] **The frame clock returns, and its consumer is declared in the same round this time.** The deletion was right and the repair is not "put `GpuTimer` back": a per-pass GPU span with no telemetry row is what got deleted, so **the row is the requirement and the timer is its implementation detail**. The published statement that spans must not be summed travels with it (§ I.11), and so does `Σpass / frameMs` as the discriminator that says whether attribution is allowed at all
- [ ] **720p60 is a frame budget of 16.67 ms and the acceptance is a percentile, never a mean** — `p50`, `p95`, `p99` over the declared path, with the count of frames over budget published beside them. *A mean hides exactly the frames a person notices*
- [ ] **Until a case exists, no line in this document may claim the target is met**, and none does today — this line exists so that a later round cannot infer it from silence. **The honest current statement is: the first three constraints are met and measured; the fourth is undemonstrated**
- [ ] `gpuFrameMs`, one pair spanning the whole encoder, so `Σpass / gpuFrameMs` says whether attribution is even allowed — TOOL, two query slots
- [ ] `frameMs − Σ(spans)` published as its own column, so "unattributed" is measured rather than subtracted by hand
- [ ] Per-pass telemetry published as a distribution instead of a mean (`FrameTelemetry.cpp:66-72`)
- [ ] Overdraw: fragments shaded per output pixel — TOOL, and it is where a forest actually costs
- [ ] Triangle size distribution in projected pixels, p50/p95 — TOOL
- [ ] Culling yield per stage: submitted against visible — TOOL
- [ ] Early rejection count — TOOL
- [x] Run identity on every line
- [ ] Every dial that changes the picture published as its own telemetry column, so two runs of one wasm hash are comparable — TOOL
- [ ] Readback of colour and depth, and a PNG writer (the deleted PNG writer, the deleted file-artefact sink, held by `Makefile` `verify-still`, which fails if a run writes no still). *Posting the artefacts to `fb-sim` was the second half of this line and `b83285f` deleted `ServerArtifacts.{h,cpp}` with the collector; the artefacts are written to `OUTSHINE_OUT` and nowhere else.* — **unbuilt**: the client that carried this was deleted with the browser-era clients; the line is scope again
- [x] `SceneRunner` executing a declared `runs` block natively and writing still and depth
- [ ] The same `runs` block executed by the wasm client, returning still and depth over HTTP — TOOL, a readback and a POST; the sink already exists
- [ ] Cross-client picture comparison on sky/not-sky coverage against a mask frozen on one side, with the self-noise floor published first — TOOL
- [ ] Randomised order within a measurement block — the counterbalanced ABBA design aliases curved drift into the treatment at unity gain and is not an instrument
- [ ] Frame-index-matched comparison on a declared path, instead of a run's p50 as the statistic
- [x] Bench as a layer over the system, never a mode inside it (`WalkBench`, `SubjectBench`, `TreeBench`)
- [ ] `verify-types`' negative gate asserting *why* it fails — any compile error passes it today

## The instrument's floor is not one number, and a comparer must say which one it is comparing at

**[MEASURED] by `board:1187`, and this is where a later round will look for it rather than in a closed
item:**

| regime | spread |
|---|---|
| within one run, by arm | **0.2 – 3.4 %** |
| between adjacent runs | **≈1 %** |
| **across a warming three-minute session** | **`fill-twice-lit` spanned 0.19 ms — 5.2 %** |

**So *the instrument's floor* is a range spanning an order of magnitude, and which value applies depends
on when the two numbers were taken.** A round comparing two commits must **quote the regime**, or it is
comparing a difference against a floor that was measured under different conditions — and a 3 % change is
a regression under one and noise under another.

**The warming term is the one that will be forgotten**: the machine gets slower over three minutes of
continuous measurement, so **a long session's later arms are systematically dearer than its earlier
ones**, and an A/B run that puts one commit first pays that as a bias. **Randomised or interleaved order
is the standard answer and this document already asks for it** — the ABBA line above — for a related
reason.

**And one limitation of the measurement that produced these numbers, recorded with them.** The
cross-commit comparison ran two `git worktree`s with today's `test/frame/` copied in and one line
stripped, so **the instrument was identical everywhere that touches a duration**. But the **published
pipeline count is absent from the two historical runs**, and the *30* they are compared against was
**read from the vertex-layout table rather than measured**. The durations are sound; **the pipeline count
attached to them is a reconstruction.**
