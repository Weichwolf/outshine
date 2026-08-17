Type: feature
Area: corpus
Tags: oracle, khronos, instrument

**The animated case tier, against the oracle**

**34 render cases and not one is animated.** The reader parses `animations` — 21 sites in
`src/gltf/Document.cpp` — `src/core/Keyframes.h` and `src/core/CatmullRom.h` exist, and
`test/outshine/unit/gltf/ASamplerIsWhatTheFileSaysBetweenItsKeyframes.cpp` holds the sampler against what the
file states between its keyframes. **So animation is built and unit-tested and has never been compared
to Cycles.** A sampler that is right about its own arithmetic and wrong about the picture is exactly the
gap the render suite exists to close.

Khronos ships the tier: `AnimatedTriangle` and `AnimatedCube` for the mechanism alone, `BoxAnimated` for
a node hierarchy in motion, `InterpolationTest` for all three of `STEP`, `LINEAR` and `CUBICSPLINE` side
by side, `SimpleSkin` and `RiggedSimple` and `RiggedFigure` for joints, `AnimatedMorphCube` and
`MorphPrimitivesTest` for weights. **The ladder grows wide before it grows tall** — every interpolation
mode gets its own basic case before a rigged figure combines them.

**What it needs that a still does not**: a declared **duration and the frame grid it is sampled on**, and
the oracle rendered at every frame of it. Blender's importer builds the animation as f-curves, so the
preparer must set the frame before each render — which is one more declared quantity in the manifest and
one more thing the recipe must digest, since a case rendered at the wrong time is a different picture with
no way to tell. **So the frame is part of the key and every frame is its own cached product**, which is
the arithmetic of `board:1129`'s decision rather than a further requirement — and it is what makes the
early exit affordable at all, since a red case must be able to stop without having paid for the frames
after its first divergence.

**Done when** each animated case declares its duration and frame grid, our renderer and Cycles are both
evaluated at every frame of it in order, and the cases sit within the picture bound on the same terms as a
still — the bound does not soften because something moved.

**How it is judged, decided at `board:1129`: frame by frame, breaking at the first failing frame.**
Both sides are evaluated at every frame of the declared duration and compared in order; the case stops at
the first frame outside the picture bound and reports **which frame** rather than a whole-sequence
statistic. **A green case pays its whole duration in oracle renders; a red one pays only up to its first
divergence** — and that frame is the most diagnostic, since later ones inherit the error.

**Which forbids the obvious shortcut**: a per-sequence aggregate — a mean over frames, a worst-of — would
let a case with one badly wrong frame average into the bound, and would name no frame to look at. **The
verdict is per frame and the report names the frame.**

## Comments

**The temporal quantity was singular in two places and is now the duration in both. THE SCOPE DID NOT MOVE
HERE — it moved at `board:1129`, and these two sentences were left behind.** What they said:

- *What it needs that a still does not: a declared **time**, and the oracle rendered at that same time … the preparer must set the frame before it renders*
- ***Done when** each animated case declares its **time**, our renderer and Cycles are both evaluated **there***

The owner decided `board:1129` **frame by frame, breaking at the first failing frame**, and the paragraph
recording that was added to this body while the two above were not. **A feature stating its requirement
twice, in two different sizes, is a feature that can be called finished at the smaller one** — a later
round reading *declares its time* would have rendered one frame per case, met the clause verbatim, and
left the interpolation error `CUBICSPLINE` exists to expose entirely unmeasured. That is exactly the
failure a *done when* is for, pointed at itself. The clause was not narrowed or widened; a second and now
false statement of the same requirement was removed.

**One consequence of the decision is now written down where it was only implied**: the frame is part of
the oracle key, so **each frame is its own cached product**. That is arithmetic and not new scope — and it
is load-bearing, because the early exit is only cheap if a red case can stop without having paid for the
frames beyond its first divergence.

**Checked rather than assumed, and clean:** this item carries **no** cost figure, product count or
cache-key statement priced at one render per case — the `858 x 8.17 s` figures elsewhere on the board are
the **still** corpus sweep in the generator items and are unrelated, and `board:0078`'s animated rungs
(6, 13 and the skinning rows) carry criteria columns only, with no render count to go stale. `board:1129`
itself is internally consistent: its options were priced before the decision and its own decision
paragraph restates the cost in the post-decision shape.
