Type: bug
Area: corpus
Tags: instrument

**An importer sits beside the corpus it serves**

A step that decides what lands on disk for ONE vendor is digested with that vendor. Adding a corpus, or
changing how one is imported, leaves every other corpus's prepared cases exactly as they were.

## What it was

`board:1451` moved the preparer's digest from *every `.py` under `test/harness/`* to *the shared
preparer plus the case's own vendor's steps*, and it worked -- for the steps that were already beside
their corpus. **The generator's importer was written into `test/harness/shared/corpus/prep/`**, which is
every case's, so it was in every case's digest.

[MEASURED] a one-line comment added to it put **all 1153 cases of the other corpora** out of date at
once. That is the exact coupling the per-case digest was built to remove, one directory short of
removing it.

## What it is

`test/harness/khronos/generator/prepare/import_cases.py`, beside the `fetch.py` that serves the same
corpus, loaded by the same positional lookup: `vendor.harness_of(root)` finds the harness and
`vendor.at` loads the step. **There is no list on either side and adding a corpus adds a directory.**

[MEASURED] the two digests are now independent -- `test/khronos/glTF` and `test/khronos/generator`
derive different sets, and neither moves when the other's importer changes.

## A defect the move surfaced, which is worth more than the move

Loading the importer from its new home ran it again, and it **walked off the end of a buffer**. The
reader assumed every accessor was tightly packed float32; `Animation_SamplerType` is a whole group
about the opposite -- a rotation may arrive as normalized bytes or shorts, and a buffer view may
interleave. It now reads `componentType` and `byteStride` from the file and scales a normalized integer
by glTF's own rule.

*The importer had been shipping wrong answers for three of the thirty-four models and only refused when
it was moved, because the models it was refused by are the ones it had never reached.*

## Comments

`wpt.py` and `test262.py` are in the same position and have the same defect waiting: both serve one
corpus from the shared preparer. They are not moved here because moving them re-prepares 975 cases for
no measured need -- but the next round that touches either should move it first.
