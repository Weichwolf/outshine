Type: feature
Area: corpus
Tags: oracle, khronos, instrument

**The animated case tier, against the oracle**

**34 render cases and not one is animated.** The reader parses `animations` — 21 sites in
`src/gltf/Document.cpp` — `src/core/Keyframes.h` and `src/core/CatmullRom.h` exist, and
`test/unit/gltf/ASamplerIsWhatTheFileSaysBetweenItsKeyframes.cpp` holds the sampler against what the
file states between its keyframes. **So animation is built and unit-tested and has never been compared
to Cycles.** A sampler that is right about its own arithmetic and wrong about the picture is exactly the
gap the render suite exists to close.

Khronos ships the tier: `AnimatedTriangle` and `AnimatedCube` for the mechanism alone, `BoxAnimated` for
a node hierarchy in motion, `InterpolationTest` for all three of `STEP`, `LINEAR` and `CUBICSPLINE` side
by side, `SimpleSkin` and `RiggedSimple` and `RiggedFigure` for joints, `AnimatedMorphCube` and
`MorphPrimitivesTest` for weights. **The ladder grows wide before it grows tall** — every interpolation
mode gets its own basic case before a rigged figure combines them.

**What it needs that a still does not**: a declared **time**, and the oracle rendered at that same time.
Blender's importer builds the animation as f-curves, so the preparer must set the frame before it
renders — which is one more declared quantity in the manifest and one more thing the recipe must digest,
since a case rendered at the wrong time is a different picture with no way to tell.

**Done when** each animated case declares its time, our renderer and Cycles are both evaluated there, and
the cases sit within the picture bound on the same terms as a still — the bound does not soften because
something moved.

**How it is judged, decided at `board:1129`: frame by frame, breaking at the first failing frame.**
Both sides are evaluated at every frame of the declared duration and compared in order; the case stops at
the first frame outside the picture bound and reports **which frame** rather than a whole-sequence
statistic. **A green case pays its whole duration in oracle renders; a red one pays only up to its first
divergence** — and that frame is the most diagnostic, since later ones inherit the error.

**Which forbids the obvious shortcut**: a per-sequence aggregate — a mean over frames, a worst-of — would
let a case with one badly wrong frame average into the bound, and would name no frame to look at. **The
verdict is per frame and the report names the frame.**
