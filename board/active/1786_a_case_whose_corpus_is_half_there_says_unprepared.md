Type: bug
Area: test, tools
Tags: gate, corpus, unprepared

# A case whose corpus is half there says UNPREPARED

Measured 2026-08-24 in a review worktree, warm nest:

```
FAIL    tools/driver/stills/StillsAreTakenAlongTheDriveForTheEye
FAIL    tools/driver/window/AWindowShowsTheRoadTheCarIsDriving
REFUSED material 'f31_interior2' names image 0, whose bytes could not be read
```

Both FAIL. Neither is broken. The prepared F31 has **no textures**:

```
$TMPDIR/outshine-prepared/tools-driver-f31/
  scene.gltf  484439   (Aug 22 11:18)
  scene.bin  30928140  (Aug 22 11:18)
  textures/   EMPTY    (Aug 24 03:35)
```

while the licensed source it is placed from still holds all five:
`~/Downloads/2014_bmw_3_series_f31/textures/f31_interior1_baseColor.png` and its siblings.

## Two defects, one shape

**1. The probe asks for one file and the case needs six.**
`tools/driver/window/AWindowShowsTheRoadTheCarIsDriving.cpp:191-202` opens `scene.gltf`, sets
`carThere`, and calls `Unprepared()` only if THAT is missing. The textures the gltf names are
part of the same corpus and are never probed, so a half-placed asset walks past the
`Unprepared` gate and dies at `CHECK(stood)` (`:233`) as a red verdict about the ENGINE.
`tools/driver/stills/StillsAreTakenAlongTheDriveForTheEye.cpp:472` is the identical shape.

board:1663 established that an unprepared verdict always names its remedy; board:1765 that a
case whose corpus is unfetched judges nothing. This is the third face: **a case whose corpus is
PARTLY there judges the engine on the missing half.**

**2. The preparer reports `placed` without verifying what it carried.**
`test/harness/shared/corpus/prepare.py:170-177`:

```python
for directory in asset["carries"]:
    source = os.path.join(found, directory)
    if not os.path.isdir(source): continue
    target = os.path.join(destination, directory)
    if os.path.isdir(target): shutil.rmtree(target)
    shutil.copytree(source, target)
```

`files` are digest-checked one by one (`:158-167`); `carries` is an `rmtree` followed by a
`copytree` with **no digest, no count and no refusal**. A source directory that is empty, or a
copy that lands nothing, still reaches `placed.append(...)` and prints success -- and it has
DESTROYED the good copy that was there, because the `rmtree` runs first.

## What will be true

- [ ] Every file a case's corpus needs is probed before the case runs -- for a glTF asset that
      is the document AND the images it names -- and one missing file is `Unprepared()` with
      the remedy, never a `CHECK` failure.
- [ ] `prepare.py` states what `carries` brought: a file count and a total size in the emitted
      record, and a refusal when the count is zero. A `placed` that placed nothing is a lie
      the gate then reports as an engine defect.
- [ ] The destructive `rmtree` happens only after the source has been proven non-empty, so a
      failed preparation cannot leave a machine worse than it found it.
- [ ] Negative control: one texture removed from the prepared directory -> the two driver cases
      report UNPREPARED naming that file, and the trailer counts it as unprepared rather than
      failed.

## Comments

- 2026-08-24, reviewer round -- found while trying to reproduce the windowed drive's WORST
  frames (board:1571). The drive suite is currently UNRUNNABLE on this machine and says so in
  the language of a broken engine, which is exactly the confusion board:1778 filed about a
  case that cannot finish: green, red and unreachable must not read alike.

## Measured, and it is worse than half-there (2026-08-24)

`test-render-outshine-grown-trs-hierarchy/` after a concurrent review:

```
0-reference.png  1-outshine.png  manifest.json  oracle.exr  oracle.materialIndex.exr
oracle.materialIndex.raw  oracle.normal.exr  oracle.normal.raw  oracle.objectIndex.exr
oracle.objectIndex.raw  oracle.raw  oracle.seed-shift.exr  oracle.seed-shift.raw
oracle.uv.exr  oracle.uv.raw  provenance.json
```

Sixteen files present. **`scene.glb` is not one of them** -- every OUTPUT survived and the one
INPUT was removed. The case reports UNPREPARED, which reads as "never fetched" when the truth
is "fetched, scored, and then pruned out from under the next reader".

And it is self-sustaining: `PruneCase` removes what the run did not touch, so once a case
goes UNPREPARED it touches nothing, and the next prune removes whatever is left. A corpus
loses ground every round it is read by a runner that cannot use it.

| | |
|---|---|
| gate before a parallel review | 235 PASS, 0 UNPREPARED |
| gate after | 231 PASS, 4 UNPREPARED |
| gate after that, with the prune lock in place | 231 PASS, 4 UNPREPARED -- the lock stops the bleeding, it does not heal it |

`board:1789` carries the sharing defect and its repair. This item carries what the case must
SAY: a directory holding sixteen artefacts and no subject is not "not prepared".

---

## The "loss" was an eviction, and both earlier readings are withdrawn (2026-08-24)

I filed this and `board:1789` calling the pruned inputs a data LOSS. They were not lost. The
prune log says so on every line it writes:

```
KEPT     10012  manifest.json    the keep set: the pictures, the declaration, the provenance, the subject
STAYS    32849  oracle.f0000.exr the store holds no object under key 17f6a834…
```

`Prune::Examine` (test/harness/shared/Prune.h:139-165) removes a file only when the content
store vouches for its bytes (`Proof::StoreHoldsTheseBytes`) or when this run's own arms wrote
it. `$TMPDIR/outshine-content` holds **1 091 objects, 586 MB**. Every pruned subject was
recoverable, locally, with no network at all.

**Measured**: 21 of 21 `grown` cases had lost `scene.glb`. Nineteen came back with

```sh
python3 test/harness/shared/corpus/prepare.py all --manifest test/render/outshine/grown/<case>/manifest.json
```

from the store, no fetch. The remaining two are GENERATED rather than fetched, and their
preparer would not build -- see below.

So the defect this item names is real but smaller and sharper than I wrote it: **a case whose
subject sits in the content store reports `UNPREPARED`, which reads as "never fetched", when
the truth is "evicted, and one local command brings it back"**. Three states wear one word:
never prepared, prepared and evicted, prepared and destroyed. The runner names the recovery
in `Prune.cpp:27-32` and the case does not.

`board:1789`'s claim -- that a parallel runner deletes another's data -- is likewise softened:
it evicts data the store still holds. The prune lock is still right (serialising eviction
keeps a reader's working set), but it stops an inconvenience, not a loss.

## And the two that would not come back

`test/harness/render/outshine/grown/prepare/grown.py:83` compiled `GrowPart.cpp` with
**`-std=c++17`** while the tree is C++23. CLAUDE.md: *"one `-std` for the whole tree"*. It
failed on `std::numbers` (C++20) and `std::span` (C++20):

```
src/core/PunctualLight.h:18:37: error: no member named 'numbers' in namespace 'std'
src/gltf/Subject.h:167:35: error: no template named 'span' in namespace 'std'
```

A second spelling of the tree's own standard, in the one script that rebuilds what the prune
evicts. Corrected to `-std=c++23`; `beech` and `cube` then regenerated and all 21 cases hold
their subject again.

---

## Reviewer round, 2026-08-24 — the value was corrected, the class was not

`test/harness/render/outshine/grown/prepare/grown.py:83` now reads `-std=c++23`. Verified.

It is still a **second spelling**. CLAUDE.md: *"one `-std` for the whole tree"*. The tree
holds exactly two:

```
test/run.sh:23                                          CXXSTD=-std=c++23
test/harness/render/outshine/grown/prepare/grown.py:83  "-std=c++23", "-O2", "-Wall", ...
```

and the second one also re-spells `-O2 -Wall -Wextra -Wpedantic -Werror -Wno-unused-parameter`,
which `test/run.sh:24-26` already declares as `WARN` and `OPT`. Nothing gates the pair. The
drift this item measured -- `-std=c++17` against a C++20 `std::span` -- can recur tomorrow and
will again be discovered as *"two cases would not come back"*, i.e. as data loss, hours later.

- [ ] `grown.py` reads the standard and the warning set from `test/run.sh` (parse the two
      assignments, or `sh -c '. test/run.sh --print-toolchain'`), so there is one spelling.
- [ ] Or a claim asserts the two spellings agree, which is the cheap form and still catches
      the drift on the next gate rather than on the next corpus rebuild.
- [ ] Negative control: `grown.py` set back to `-std=c++17` -> red in the fast gate, not in a
      sporadic corpus round.
