Type: task
Parent: 1128
Area: corpus
Tags: oracle, khronos, instrument

**One animated glTF against the oracle, frame by frame**

`board:1128` is the owner's named target and has **no task under it**. This is the smallest thing that
puts an animated glTF in front of the oracle, and it forces into existence the one link the tree is
missing.

**THE MISSING LINK, MEASURED.** The sampler exists at four layers — `core/Keyframes`, `core/CatmullRom`,
`gltf/Track`, `scenario/Animation` — and `Animation::At(Target, double frame)` is **already
frame-indexed**, which is the currency `board:1129` decided in. What does not exist is a **consumer**:
`git grep Animation -- src/clients/ src/render/` returns one *comment*. **Nothing takes a time, samples
the tracks and updates a draw list.** A case cannot pass without that link, and the link is unproven
without the case, so they land together.

**The subject is `BoxAnimated`**, which `board:0078` already ranks at rung 6: *time, and nothing else —
one object, TRS, no light.* One object, node TRS, no skinning, no morph, no material stack — so a
disagreement has one candidate cause. `InterpolationTest` is the same rung and is the second case, not
this one: it varies `STEP`, `LINEAR` and `CUBICSPLINE` at once and cannot attribute a residual until the
mechanism itself is green.

**The verdict shape is `board:1129`'s and it is binding**: both sides evaluated at **every frame of the
declared duration, in order**, stopping at the first frame outside the picture bound and reporting
**which frame**. No per-sequence aggregate — a mean or a worst-of would let one badly wrong frame average
into the bound and would name no frame to look at.

## Acceptance, and every clause can fail

- [ ] **The case declares a duration and a frame grid**, not a time, and both are in the oracle key — so
  **every frame is its own cached product**. `board:1128` carries that as arithmetic, and it is what makes
  the early exit affordable: a red case must stop without paying for the frames beyond its divergence
- [ ] **The subject moves.** The drawn transform at frame *n* differs from frame 0 by more than the
  instrument's own floor, published. **A case that renders the rest pose 30 times passes every
  frame-by-frame comparison** — this is the hollow green of an animated suite and the clause that stops it
- [ ] **The first failing frame is named**, and a green case reports the frame count it compared
- [ ] **Determinism across the sequence**: the same declaration rendered twice in one process gives a
  byte-identical scene-linear readback **at every frame**, not only the first
- [ ] **`SceneVelocity` is non-zero where the subject moved**, and this is the first case in the tree that
  can say so. Every geometry stage contributes that target and the temporal resolve reads it; with nothing
  ever moving, **no case has produced a non-zero velocity and the temporal stage's correctness has never
  been measurable.** Publishing the covered-pixel count with a non-zero velocity is nearly free here and
  it retires a blind spot the still suite cannot reach

**What it must not do.** It must not gain a second oracle recipe, a second camera path or a second asset
to explain a residual. One subject, one path, the **frame** as the independent variable — the same shape
`board:1162` uses for content and `TheVisibilityTermIsPricedPerRay` uses for lights.

**Done when** `BoxAnimated` is compared to Cycles at every frame of its declared duration, the consumer
that drives a draw from a time exists and is cited from this case, and a deliberately wrong frame index is
shown to fail it.
