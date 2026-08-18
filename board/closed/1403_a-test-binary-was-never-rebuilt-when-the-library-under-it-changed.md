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
