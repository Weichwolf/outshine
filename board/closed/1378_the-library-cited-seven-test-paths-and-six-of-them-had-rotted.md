Type: bug
Area: core
Tags: instrument

**The library cited seven test paths and six of them had rotted**

`CLAUDE.md`: *the engine is a library and it is platform agnostic -- it declares what it needs from a
host and calls nothing else; everything that runs it is a test.* Nine comment sites in `src/` named a
file or directory under `test/`, which reverses that relation.

## The measurement, and it is why this is a bug rather than a preference

[MEASURED] seven distinct paths were cited from `src/`. **Six did not resolve:**

| cited | resolves |
|---|---|
| `test/shader/BothHalvesOfTheBrdfAgree.cpp` | no |
| `test/shader/ABackFaceTurnsTheWholeTangentFrame.cpp` | no |
| `test/shader/AnExactRayAgreesOnBothSides.cpp` | no |
| `test/unit/render/stages/` | no |
| `test/frame/` | no |
| `test/mods/demo/mod.json` | no |
| `test/run.sh` | yes |

They had all moved under the suite reorganisation and nothing moved the comments with them. **A
citation pointing from the stable tree at the moving one has no owner**, which is the same argument
the board already carries one level up -- *THE CODE CITES THE REQUIREMENT; THE BOARD NEVER NAMES THE
CODE*. A `board:` marker travels with the line it sits on; a path is a copy that can drift, and did.

**`EveryPathCitedInADocumentResolves` did not catch it** because its population is the documents, and
these were in source.

## What was NOT done, and the reason is the instrument's domain

`Blender`, `Cycles` and `oracle` still occur in `src/` and were deliberately left. Where they occur
they carry **the origin of a number** -- *MEASURED against `NormalTangentMirrorTest`, whose `TANGENT`
Blender's own exporter wrote* -- and `CLAUDE.md` requires every number to carry its origin. **An
external renderer that once produced a measurement is a source, not a dependency**, and scrubbing the
word would delete provenance to satisfy a rule it does not breach. Four comments that read as though
the ENGINE were justified by the oracle -- the framing lens, the raster row order, two vocabulary
notes -- were turned so the reason is the engine's own.

## Comments

**The engine's four framing constants were the one place this could have been more than a comment.**
`kFramingFocalLengthMm = 50` and `kFramingSensorHalfHeightMm = 12` were annotated as *Blender's factory
50 mm lens*, which reads as a value copied from the oracle. It is not: the engine computes
`2*atan(12/50)` from its own two `[SET]` numbers and the manifest declares its camera independently,
with `ADerivedCameraIsTheFramingRuleAndNotAQuotation` refusing a mismatch. **The value was already the
engine's and only the sentence was borrowed.**
