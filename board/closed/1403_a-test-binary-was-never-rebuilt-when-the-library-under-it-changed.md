Type: bug
Area: harness
Tags: instrument

**A test binary was never rebuilt when the library under it changed**

`test/run.sh` decides a binary is fresh by reading the compiler's own `.d` file and comparing every
prerequisite's mtime against the binary. **`-MMD` records the HEADERS a translation unit read. The
library's objects are LINK inputs and appear in no `.d` at all** -- so a change to any `.cpp` under
`src/` left every test binary that links it untouched, and the suite reported **green against a
library the binary was not built with**.

*The `*.o` arm of that loop was written for exactly these and never saw one.*

## Measured, and the blast radius is not the whole tree

| binary | source changed | rebuilt |
|---|---|---|
| `harness-khronos-glTF-ScoreEveryKhronosCase` | `src/render/stages/SubjectDraw.cpp` | **yes** |
| `harness-khronos-glTF-ScoreEveryKhronosCase` | `test/harness/shared/render/Parity.cpp` | **yes** |
| a unit test | `src/gltf/Document.cpp` | **NO** |

**So the corpus measurements stand and the unit tree's did not.** The corpus scorer compiles the
sources it needs into its own translation unit; a unit test links the objects.

## How it was found, and it is the ordinary way

A shader test failed with `use of undeclared identifier 'transmitted'` after the identifier had been
moved to the top of the emitted text. Moving it again changed nothing; the error kept the same line
number. **A stale binary is what reports an identical error against changed source**, and the
identical line number is what said so.

## The repair

`Fresh()` now walks `$OBJECTS` as well as the `.d`, so every object the binary links is a
prerequisite. [MEASURED] after it, a change to `src/gltf/Document.cpp` rebuilds the unit tests.

## Comments

**This is the most dangerous class of defect an instrument can have and it was invisible by
construction**: it makes a test greener than the truth, never redder, so nothing in a run's output
points at it. It was found only because a change that HAD to alter a result did not.

## What it had been hiding, found in the first run after the repair

**A `static_assert` that contradicted the format.**
`test/outshine/unit/generators/SameRegionSamePlacement.cpp` asserted that a pane with transmission 0.9
and ior 1.5 is `Refractive`. `KHR_materials_volume` says a material with no thickness is thin-walled
whatever its index of refraction, and `GlassBrokenWindow` at the pin declares exactly that pane. The
assertion encoded the rule the engine used to have; it now states the format's, and a second material
given a thickness carries the volume case.

**Two frame tests reading paths that stopped holding products.**
`TheFrameCostIsPublishedAgainstItsOwnFloor` and `TheVisibilityTermIsPricedPerRay` still named
`test/khronos/glTF/<case>/scene.gltf` -- the tree -- although `board:1364` moved every product to the
prepared root. **They would have said so on the day of the move.** Their binaries were never rebuilt,
so they went on reporting whatever they had last been built to report.

*Both are pre-existing and neither is large. What matters is that the first honest run surfaced both
at once, which is the shape a hidden instrument defect always has: it does not cause failures, it
banks them.*
