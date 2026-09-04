Type: bug
State: open
Area: test, gate
Tags: measured, gate

# `make test` is green, and it runs inside the bound it declares

**Benchmark** -- Unreal's automation gates on a green run and treats a slow test as a defect the
same way it treats a failing one; RAGE's build farm does the same. **Both agree**: a gate that is
red at HEAD is a report nobody reads, and the day something real breaks it looks like today.

## Where it stands, measured 2026-09-04, the claims arm of `make lint`

Ten claims are red (twenty arms, plain and `~sanitised`). Each has a cause now and none has a
repair. The right-hand column is the work:

| claim | cause | the repair |
|---|---|---|
| `AnItemReachesClosedThroughActive` | board:2125 was deleted saying `State: open` (fa9534b5), against a ceiling of 0 | the ceiling becomes 1 with the commit named on the line; the habit is the fix |
| `EveryItemNamesTheBenchmark` | items 2117-2124 carried a heading instead of the `**Benchmark**` paragraph | every item on the board carries the paragraph -- done in the board rewrite that filed this line |
| `TheBuildDeclarationAuditsItself` | `src/client/Main.cpp` and `PlaceCamera.cpp` are linked by no suite | the client's suite lists them, or `outshine/places` does |
| `TheNestRefusesASecondRunner` | a NESTED invocation on the inherited nest is refused instead of passed through | `run.sh`'s lock recognises its own parent's claim |
| `TheEngineNamesNoSubject` | 153 subject nouns in `src/engine` + `include/`; `Kerb` rose 5 -> 6, `Carriageway` fell 4 -> 0 unrecorded | board:2101 takes the count to 0; the fallen number is recorded where it fell |
| `NoFramePathCallReachesABlock` | the physics seed reaches `malloc`; the picture reaches `Readback::FromBuffer/Land/Release` | board:2104 (the heap), board:2130 (the readbacks are instruments and leave the picture's reach) |
| `APreparedFileNeverLandsInTheTree` | 148 files under the case trees are neither manifest, reference nor `.gitignore` (`test/khronos/glTF/*/reference.f000N.png`) | the claim says what a multi-frame reference is, or the files go |
| `TheCorpusIsRebuiltByOneCommand` | 1426 manifests standing, 1424 planned | the preparer plans the two it skips, or they leave |
| `AGreenTrailerNamesWhatItDidNotJudge` | `test/opendrive`, `test/clearsky`, `test/scripts` hold no fetched subject and the runner is silent | the runner names them in its trailer |
| `EveryRenderNamesItsIndices` | 0 render rows in the prepared corpus, so the claim counts over nothing | the corpus renders, or the claim refuses UNPREPARED rather than FAIL |

Beside the claims: `README.md` names `STATE.md`, `apps/`, `test/gate.sh` and `--audit-layers`,
none of which exists as described, and `mesh.xml` / `r.xml` are tracked at the root with no
reader. Both are the same rule -- the tree describes itself truthfully -- and both are one commit.

## The clock

`kFastGateBoundMs = 230000` in `test/run.sh` and every harness layer builds twice (plain and
sanitised). The choice is stated: the bound rises with its reason, or the sanitised set shrinks
to the layers whose findings are worth the seconds. Not resolved by turning the sanitiser off.

## What will be true

- [ ] `make test` prints no RED case and no overrun line
- [ ] One deliberately broken oracle turns it red again
- [ ] `README.md` names only what exists; no artefact is tracked outside `build/`

## Ruled out

CentralPark's `Variation < 1.0` refusal: the owner has ruled the guard correct. A hundredth under
the bar during a heavy refactor is a deviation to live with, not a bar to lower.
