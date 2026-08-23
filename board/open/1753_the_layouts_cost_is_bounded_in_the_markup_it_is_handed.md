Type: bug
Area: ui
Tags: performance, layout, unbounded

**The layout's cost is bounded in the markup it is handed**

`Placer::Place` re-walks each child's whole subtree up to FOUR times before placing it,
so the cost of a nested flex tree is exponential in nesting depth, not linear in boxes:

| site | call | what it does |
|---|---|---|
| src/ui/Layout.cpp:690 | `Measure(one.Node, ...)` | :169-174 -> full `Place` of the subtree |
| src/ui/Layout.cpp:745 | `BaselineOf(one.Node, ...)` | :348-350 -> full `Place` of the subtree |
| src/ui/Layout.cpp:799 | `BaselineOf(one.Node, ...)` | the SAME subtree, laid out a second time |
| src/ui/Layout.cpp:815 | `Place(one.Node, ...)` | and then for real |

Nothing memoises: `MinContent` (:187), `MaxContent` (:236), `BaselineOf` (:348) and
`Measure` (:169) all recompute from scratch on every call.

Measured (probe, `<div style="display:flex;align-items:baseline">` nested d deep, one text
run at the bottom, 800x600 viewport, -O2, AhemFont):

| depth | boxes | Layout::Build | ratio to previous |
|---|---|---|---|
| 8 | 8 | 26.429 ms | 4.11 |
| 9 | 9 | 119.677 ms | 4.53 |
| 10 | 10 | 375.886 ms | 3.14 |
| 11 | 11 | 1589.635 ms | 4.23 |
| 12 | 12 | 6464.196 ms | 4.07 |

Growth base, geometric mean over depths 7..12: (6464.196/6.431)^(1/5) = 3.99 per level --
exactly the four re-walks the table above names. TWELVE boxes cost 6.5 seconds; depth 16
extrapolates to 6464 x 3.99^4 = 1.64e6 ms = 27 minutes.

This is the tree's own rule broken -- "bounded terms on the frame path (no alloc/lock/disk/
unbounded block)" -- on a surface a SCENARIO declares (`Live::Compose`, src/clients/Live.cpp:
438-462, runs the whole chain per `Redeclare`). Nothing in test/unit/ui measures cost at
all, so the cliff is invisible to the gate.

What will be true:

1. Layout is linear in boxes for a tree of bounded width: the intrinsic sizes
   (`MinContent`, `MaxContent`), the baseline and the measured size are computed ONCE per
   node per Build and cached beside the box -- the browser form (a per-node intrinsic-size
   cache keyed by the available width).
2. A unit case in `test/unit/ui/` lays out a nesting ladder and asserts the cost is within
   a stated multiple of the box count -- a test that would fail today at depth 12.
3. `Layout::Build` publishes a count a scenario suite can assert on (nodes placed, places
   per node), so a regression to re-walking is a number, not a stopwatch.
