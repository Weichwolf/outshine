Type: issue
Area: scenario
Tags: perf, instrument, scope

**The scenario suite has no members, and a Blender open movie is the subject it lacks**

`CLAUDE.md` declares five suites split by instrument and draws the fifth without a directory:
**scenario** — *the floor broke, the run was not deterministic, memory grew* — decided by p50/p95/p99
over a moving camera, determinism, residency and memory. **It is declared and it has no members**, which
is why the fourth constraint, *this device at 720p60*, is the least measured of the four.

**The owner's proposal, and it is phase two**: take Blender's CC open movies, get their content into
this engine, and play it — because a complex scene is what forces the compositor and the renderer to
optimise. Phase one stays what it is: all 148 corpus models green on both counts (`board:1171`).

## What the proposal is, stated so it is not mistaken for the impossible one

**The subject is the `.blend`, not the released video.** The Blender Open Movie projects publish their
**production files** under CC-BY, so the step is an **export** and not an analysis. *Recovering geometry,
materials and lights from pixels is inverse rendering and nothing this tree could put behind
`prepare.py`; that is not what is proposed and this paragraph exists only so a later reader cannot read
it in.*

## Why this subject and not a synthetic one, and the reason is the oracle

**These films were rendered with Cycles.** The oracle this tree already pins, already measures the
limitations of, and already caches by recipe is the same renderer that produced the reference the world
knows these scenes by. **No synthetic scene can offer that**, and it is worth more than the polygon
count: it means the ambition target and the correctness oracle are the same instrument, so a gap between
them is a statement about this engine and not about two renderers disagreeing.

## Three things that must be settled before anything is built, and the third is the decision

**1. glTF is a lossy container for a film scene, and the loss sits where the picture is.** No volumetrics,
no particle systems, no hair or fur primitive, and **no Cycles node graphs** — glTF's material model is
fixed by design, which is the same door it closed when it removed shaders from content. A haired
character bakes to curves in the millions or loses the hair. **Whatever survives the export is what this
engine is asked to draw, and it is not the film.**

**2. So the film's own frames are NOT the reference.** The comparison stays exactly what
`CLAUDE.md` already specifies: **Cycles on the exported glTF against us on the exported glTF**, one
recipe, one cache. The released frames are an *achieved result* in the sense the reference table uses —
cited for what was reached, never as authority over a measurement.

**3. THE DECISION, and it is the owner's.** *Many* glTFs rather than one is already the right call and
is not what is in question — a film setup does not belong in a single file, and the engine does not want
it there: `kind = gltf-file` with `params = content hash plus which primitive` makes each exported file
an ordinary part, and many parts with declared placements are the `declared` compositor **past its
degenerate case**, which is the whole point of the exercise. **What is undecided is the AXIS of the
split.**

| axis | what it exercises | what it costs |
|---|---|---|
| **by object or collection** *(recommended)* | the compositor entire — part store, the key, budget quantisation, the completion queue, residency and eviction — because the camera moves through a scene that is held rather than reloaded | the export must carry animation, which this tree can already read: skinning, morph targets and all three interpolations are delivered, each with a green render case |
| **by shot** | the above, plus a declared discontinuity where residency legitimately resets | a shot boundary has to be declared. It is **packaging over the first axis**, not a rival to it, and falls out once the first is built |
| **by frame** | the file reader, repeatedly | **nothing the compositor exists for.** Streaming, residency, eviction and quantisation are all about holding a scene WHILE a camera moves; re-reading per frame skips every one, and the data is the whole scene times the frame count |

**The recommendation is by object or collection, packaged by shot.** *Splitting by frame is the one axis
that would make the scenario suite a measurement of the file reader.*

## What this issue does not decide, and says so

**Which film.** Complexity is the point, so the smallest one that still has instanced vegetation, a
rigged character and a moving camera is worth more as a first member than the most spectacular one.
That is a task's question once the unit above is settled.

**Licence.** `board:1171` records the owner's ruling that licence does not gate the corpus because
nothing is redistributed. **The same reasoning covers this**, and CC-BY would be satisfiable anyway.

## Comments

**This issue is filed and worked around, never waited on.** Phase one has 53 core models with no case at
all and ten rows this ruling just moved out of `REFUSED` (`board:0079`), so nothing here blocks and the
board cannot run out of ready work while it stands open.

## The owner sharpened it: a still at time X, and that makes it TWO members rather than one

**The proposal as refined**: Cycles on the `.blend` as the reference, this engine on the exported glTF,
**comparing single images at one declared time**. That is the relationship this tree already runs — a
case is a directory, the oracle is cached by a key over the whole declared scene — with a subject two
orders of magnitude larger. **The machinery does not change; the scale does, and the scale is the
point.**

**It is the `render` suite, and saying so keeps a later reader honest.** A still at time X decides
*wrong pixels*. The frame floor, determinism, residency and memory decide over a **moving** camera, which
is a different instrument on the same subject. **Both are wanted and neither substitutes for the other** —
a green still at film scale is not a compositor proof, and this item exists because the scenario suite
has no members.

## Three images, not two, and the third is what makes any of it decidable

```
A   Cycles on the ORIGINAL .blend        at time X
B   Cycles on the EXPORTED glTF          at time X
C   outshine on that same glTF           at time X
```

| difference | what it measures |
|---|---|
| **B − C** | **our error.** One scene, one declaration, two renderers — the relationship every case in this tree already uses, and the only one permitted to judge the engine |
| **A − B** | **the export loss, as a number.** Volumetrics, particles, hair, Cycles node graphs — everything glTF cannot carry lands here and nowhere else |
| A − C | the sum. **Ambition, never a verdict**, because it folds two causes into one figure |

**Without `A − B` every disagreement is ambiguous, and its obvious reading — *our renderer is wrong* — is
the wrong one.** With it the export loss becomes a measured term instead of a caveat, and it answers the
selection question in advance: **a `.blend` with no volumetrics, no particles, no hair and Principled-BSDF
materials exports very nearly losslessly**, because Blender's exporter maps Principled onto glTF's
metal-rough almost one to one. *So the approach does not fail in principle — it fails per shot, and that
is checkable before a frame is rendered.*

**This is the same shape as `board:1204`.** There the question was whether the oracle honours what the
file declares, asked before a row was built rather than discovered after. Here it is whether the EXPORT
preserves what the oracle will be shown — the same question one step earlier in the same chain, and
`A − B` is the instrument for it.
