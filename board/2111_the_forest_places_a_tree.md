Type: bug
State: open
Area: generators
Tags: measured
Depends: 2108

# The forest places a tree

**Benchmark** -- Unreal's procedural foliage spawner reports what it spawned and what it rejected
per spawner, so a spawner that yields nothing shows up as a zero the author sees. RAGE's prop
placement is authored, so an empty result is a content bug found in the editor. **Neither ships
a placer that silently returns empty**, and this one does.

## Where it stands, measured 2026-09-03 (no run since publishes the line)

```
  OldTown      building placed 1275, flora placed 0
  Jura         building placed   32, flora placed 0
  Kaiserberg   building placed  740, flora placed 0
```

`Forest` is registered, is leased a region, is asked, and places nothing -- including at the
place named for the forest it stands in. Everything downstream is unmeasurable while it holds:
the generator RANKS cannot be tested because only one subject ever takes ground, and CLAUDE.md's
first budget line -- *high geometry with RECURSIVE generators* -- has no tree to spend on.

## Read rather than measured, and what it rules out

`Forest::Occupy` -> `Consider` (`generators/flora/Forest.cpp:103-186`):

| candidate | verdict |
|---|---|
| maker order misaligned with yields | out: both walk `Entries_` order |
| region full | out: 4096 bodies, buildings take at most 1275 |
| density row mismatch | out: `Evaluate` returns a template index and `PerM2_` is built per template row |
| density resolves to zero | out: forest rows 0.028-0.033 /m², ~31 % of cells pass the draw |
| treeline, slope | out: 1980 m top at OldTown's latitude; slope max 90° |
| `NoTemplate` -- every sample off the class grid | **the leading candidate, unmeasured**: `Ground::CoverAt` projects `Region_.Geo(at)` into `Classes_->Frame()`; a frame/region origin disagreement makes EVERY sample return -1 |

**And the diagnostic that decides it already exists and is read by nobody.** `Forest` counts
eight Notes -- `noTemplate`, `noDensity`, `full`, `offRegion`, `tooSteep`, `aboveTreeline`,
... -- into `Yield::Notes_`, and `Yield::Notes()` has zero readers in `src/` and `test/`.
`Asking.cpp:174-188` publishes only Placed, Occupied and Outside. `Forest.cpp:180` also ends
the whole tile silently on `Full`.

## The solution

1. `Asking.cpp` reads `Notes()` into the ledger, one line per note per generator -- board:2108's
   pull, applied here first because it is the cheapest measurement in the tree
2. `shots --measures Jura` then names the cause in one run, and the repair is whatever that
   line says; if it is `noTemplate`, the frame the cover is sampled in and the frame the region
   was leased in are made ONE frame, passed rather than reconstructed

## What will be true

- [ ] `flora placed` above zero at Jura and Kaiserberg, and a picture with trees in it, looked at
- [ ] Every Note a generator counts stands in the ledger
- [ ] The ranks become testable: swap flora and building and the two placed counts MOVE
- [ ] Negative control: lease the region in the wrong frame and `noTemplate` reads the cell
      count

## What will show I was wrong

`noTemplate` reads 0 at Jura and the trees are still missing. Then the cause is in `Consider`'s
draw or in the sink, and the Notes say which.
