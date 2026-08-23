Type: bug
Area: scenario
Tags: performance, reader, unbounded, xml

**The scenario reader is linear in the rows it reads**

Every collection in `ReadScenarioInto` is walked with the same idiom — `Count(name)` in the
loop CONDITION, `At(name, index)` in the body:

```cpp
for (size_t at = 0; at < instances.Count("instance"); ++at) {   // ScenarioRead.cpp:399
  const Xml::Ref one = instances.At("instance", at);            // ScenarioRead.cpp:400
```

and both sides of that idiom are a full sibling walk:

- `Xml::Ref::Count` (src/core/Xml.cpp:125-136) walks `FirstChild`→`NextSibling` to the end,
  memcmp per sibling — and it is re-evaluated on EVERY iteration because it sits in the
  loop condition.
- `Xml::Ref::At` (src/core/Xml.cpp:138-151) restarts at `FirstChild` and walks until it has
  counted `which` matches.

So one collection of k rows costs 2·k² name comparisons. The idiom appears 36 times across
src/scenario/ScenarioRead.cpp (`grep -c 'Count("' `), i.e. every list a scenario declares:
instances, placements, assets, surfaces, kinds, volumes, sounds, table rows, table cells,
contacts.

**Measured** (probe: `<scenario><instances>` with n `<instance of=… id=… x= y= z=/>` rows,
`clang++ -std=c++23 -O2`, this machine, one run each — the population is one document per
n, single-threaded, warm):

| n rows | ReadScenario | µs per row | ratio to previous |
|---|---|---|---|
| 500 | 1.50 ms | 3.00 | — |
| 1000 | 4.59 ms | 4.59 | 3.06 |
| 2000 | 18.80 ms | 9.40 | 4.10 |
| 4000 | 74.04 ms | 18.51 | 3.94 |
| 8000 | 318.85 ms | 39.86 | 4.31 |
| 16000 | 1268.70 ms | 79.29 | 3.98 |

Geometric mean of the ratios over the five doublings: (1268.70/1.50)^(1/5) = 3.85 — a clean
quadratic (a linear reader doubles). Extrapolated: 64 000 instances = 1268.70 × 4² ≈ 20 s to
read one file. `board:1480` sets the bar at "as large as Fallout 4"; a city block of parked
cars is already four figures of instances.

This is not a frame-path term, but it is the load-time face of the same rule the tree holds
everywhere else: **bounded terms, batch over per-item**. And it is invisible today because
every committed scenario is tiny.

## What will be true

1. Each collection is read in ONE pass over the parent's children —
   `for (Xml::Ref one = parent.First(); one.Valid(); one = one.Next())`, dispatching on
   `one.Name()` — so reading a scenario is linear in its elements. The `Count`/`At` pair
   stays for the singleton lookups where k is 1.
2. `Xml::Ref` gains the C++23 form the loop wants: a `std::ranges`-compatible children view
   (`parent.Children()` / `parent.Children("instance")`), so the reader spells iteration
   once and no call site can reintroduce the index walk.
3. A unit case in test/unit/scenario/ reads a generated document of 16 000 rows and asserts
   the cost against a stated multiple of the row count — the same document that takes 1.27 s
   today. A wall-clock assertion alone is weak; publish the count (`Xml` sibling-steps taken)
   and assert on THAT, so the regression is a number and not a stopwatch.

## Comments

- 2026-08-23 -- repaired. `Xml::Ref` gained the range the loop wanted:
  `parent.Children("instance")` is a `std::ranges::forward_range` over the named siblings
  (`static_assert` in Xml.h holds it there), and all 36 index loops in `ScenarioRead.cpp`
  became one pass. The `Count`/`At` pair stays for the singleton lookups; the two existence
  checks became `Declares(parent, child)`, which stops at the first hit instead of counting
  to the end. `ReadScenarioInto` gained an overload taking an already-parsed `Xml`, which is
  also the natural form for reading layer over layer.
- `Xml` now publishes `SiblingSteps()` -- every step over `NextSibling`, in `Count`, `At`,
  `Next` and the new iterator -- so the claim is a COUNT and not a stopwatch.
- **Measured**, 16 000 `<instance>` rows, same machine, one document:

  | | sibling steps | steps per row | wall clock |
  |---|---|---|---|
  | index idiom (one collection restored) | 384 040 026 | 24 002.50 | 785.51 ms |
  | one pass | 32 026 | 2.00 | 9.71 ms |

  81 x fewer wall-clock ms and 12 000 x fewer steps, from ONE collection put back. The
  per-row term is now a constant: 2.00 steps, one for the grammar walk and one for the read.
- **Proving test**: `test/unit/scenario/TheReaderIsLinearInTheRowsItReads` -- reads 16 000
  rows and asserts `steps <= 4 x rows`, the bound stated with its headroom over the measured
  2.00.
- **Negative control**: the `instance` loop alone put back to
  `for (size_t back = 0; back < instances.Count("instance"); ++back)` ->
  `FAIL **THE READER IS LINEAR IN THE ROWS IT READS**` at 24 002.50 steps per row. Reverted,
  green again.
- Gate 227/227.
