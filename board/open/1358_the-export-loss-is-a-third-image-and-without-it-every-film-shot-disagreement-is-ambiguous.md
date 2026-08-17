Type: feature
Area: corpus
Tags: oracle, instrument, scope

**The export loss is a third image, and without it every film-shot disagreement is ambiguous**

The owner's comparison, verbatim:

```
A   Cycles on the ORIGINAL .blend        at time X
B   Cycles on the EXPORTED glTF          at time X
C   outshine on that same glTF           at time X
```

**Every case in this tree today is `B` against `C`.** One scene, one declaration, two renderers — which
is the only relationship permitted to judge the engine, because both sides were shown the same thing.
`A` is new, and it is what makes a film shot usable at all.

| difference | what it measures | what it may decide |
|---|---|---|
| **B − C** | **our error** | the engine. This is the picture bound and the Khronos criteria, unchanged |
| **A − B** | **the export loss**, as a number rather than a caveat — volumetrics, particles, hair, and Cycles node graphs that glTF's fixed material model cannot carry | **nothing about this engine.** It is a property of the exporter and of the subject |
| A − C | the sum | **nothing at all.** It folds two causes into one figure, and its obvious reading — *our renderer is wrong* — is the one that costs the round |

## Why this is a feature and not a note on `board:1209`

**`board:1209` is an issue: it asks which axis a film scene is split along, and that is the owner's.**
This is not that question. **`A − B` is an instrument, it is wanted whatever the answer to `1209` is, and
it is the same shape as `board:1204`** — *does the oracle honour what the file declares*, asked before a
row is built rather than discovered after. Here it is asked one step earlier in the same chain: **does
the EXPORT preserve what the oracle will be shown.**

## What it buys before a single film frame is compared

**It turns shot selection from a judgement into a threshold.** A `.blend` with no volumetrics, no
particles, no hair and Principled-BSDF materials exports very nearly losslessly, because Blender's
exporter maps Principled onto glTF's metal-rough almost one to one. **`A − B` says which shot that is,
per shot, before anything expensive happens** — and it says it in the same units the picture bound
already uses.

**And it protects the counts.** Without it, a film case that misses the bound reads exactly like a
corpus case that misses the bound, and the two counts `board:1171` fixes as the finish line would start
carrying a population they were never defined over.

## What is not decided here

- [ ] **Whether `A` is even reachable from the preparer.** `prepare.py` drives Blender already, and
  rendering a `.blend` at a stated frame is less work than the glTF path it runs today — but the
  **recipe key** must then cover the `.blend`'s own bytes, and a film project is not one file. *That is
  the first thing to measure and it may be the whole difficulty.*
- [ ] **What `A − B` is compared with.** It is not a pass/fail against the picture bound: an export loss
  is expected and its size is the answer, not a verdict. **A threshold that disqualifies a shot is a
  separate declaration** and it belongs to whoever selects shots
- [ ] **Whether `A` needs the film's released frame at all.** It does not: `A` is our own Cycles render
  of the `.blend`, under our recipe, at our sample count. The released frame is an *achieved result* in
  the sense `CLAUDE.md`'s reference table uses — cited for what was reached, never as authority over a
  measurement

## Comments

**This is filed early and deliberately thin.** It is phase two and phase one is 118 models without a
case; what it must not do is arrive after a film shot has already been compared two ways and a round has
been spent on an ambiguous number. *The cheap half — writing down that the third image exists — is the
half that prevents that, and it is done here.*
