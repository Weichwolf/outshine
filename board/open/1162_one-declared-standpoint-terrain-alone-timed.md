Type: task
Parent: 0058
Area: scenario
Tags: perf, instrument
Depends: 1161

**One declared standpoint, terrain alone, timed**

The first member of the `scenario` suite, and the fourth constraint's first real number. `board:0058`'s
subject clause asks for *a declared world scene with terrain, vegetation and buildings, at 720p, over the
orbit that already exists* — **this task is deliberately narrower, and the narrowing is the design.**

## Why not the whole scene at once

Terrain **and** vegetation **and** buildings **and** streaming **and** sky is four or five unknowns in one
number. `board:1127` is this tree's own lesson on that: a case measuring several things at once cannot
attribute its residual, and the round is spent finding out which. The asymmetry decides it — **if the
frame fits, one case answers everything; if it does not, one case answers nothing** — and the prior on a
world that has never run once is not that it fits.

**But the answer is not two scenes, because a second scene is a second unknown.** It is **one scene, one
camera path, one binary, and the DECLARED CONTENT SET as the independent variable** — which is exactly the
design `test/outshine/frame/TheVisibilityTermIsPricedPerRay` already uses, where the independent variable is the
light count and everything else is held. The engine already makes it declarable: `CLAUDE.md` states the
five geometry units are *independently declarable, one shared LOD cut, so a coverage case can ask for
subjects alone.* **Terrain alone is the first rung; vegetation and buildings are further declarations of
this same case, in later tasks, and each arrives as a difference against a measured baseline.**

## The subject

**`test/outshine/mods/demo`'s horizon-facing `run` scene** is the candidate: `fovDeg 60`, `pitchDeg 0`,
`eyeM 1.7` — a person standing on the ground looking out, which is the picture this engine is judged on.
The other three mods (`ardeche`, `badwater`, `preikestolen`) declare `pitchDeg -90` at 256×256; those are
height probes and not pictures. **The scene is read from the declaration, never built in the test** — the
mod reader exists and is unit-tested, and this is its first consumer.

## The acceptance, and every clause can fail

**Population**: **at least 240 timed frames** at **1280×720** over the declared path, one process,
**no sanitiser**, matching the frame suite's existing shape rather than inventing a second one.

**Verdict A — the instrument is sound. This is the gate and it is a real pass/fail.**

- [ ] **The frame is not empty.** The depth attachment shows a declared minimum covered fraction of the
  frame. **A world that streams nothing renders fast and passes every timing test** — this clause is what
  stops the first world number being hollow, and `board:1158` is the same lesson filed one level up
- [ ] **The picture is a function of the declaration.** Two runs of one declaration in one process produce
  a **byte-identical** scene-linear readback. If pace decides the picture, the coupling is a bug and this
  says so on the round it appears
- [ ] **Residency is bounded**: peak host and device bytes published against a **declared** ceiling, not a
  discovered one. A run that grows without bound fails here rather than by exhausting the machine
- [ ] **Attribution is admissible or it is refused**: `Σpass ≤ gpuFrameMs`, with `frameMs − Σ(spans)`
  published as its own column. Where the sum exceeds the frame, the case says attribution is not allowed
  rather than publishing a breakdown nobody may read

**Verdict B — the number, published and NOT gated on this task.** `p50`, `p95`, `p99` frame time in ms
over the population above, and **the count of frames over 16.67 ms** beside them. It is reported because
**the fourth constraint's first honest statement is that it is measured, not that it is met** — and a gate
that goes red on the first world scene and stays red for months is a test people learn to ignore
(`board:1160` names that shape). **The two verdicts are published side by side and neither is quotable as
the other**, which is the discipline the render suite already keeps between *criteria met* and *cases
within the picture bound*. A later task declares the budget as acceptance; this one earns the right to.

**And the failure mode this must not have**: a case that reports a beautiful distribution over a frame
that drew nothing, or over a path that never left the origin. Clause one and clause two exist for exactly
that, and the covered fraction is published beside the timings so a reader can see what was timed.

## Done when

the scenario suite has its first member; one declared standpoint renders terrain at 720p over the declared
path; Verdict A passes or names which clause failed; and the fourth constraint has a number with its
population, its instrument and its floor beside it for the first time.
