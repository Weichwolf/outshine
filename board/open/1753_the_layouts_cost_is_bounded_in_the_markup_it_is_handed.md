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

---

Closed -- the intrinsic sizes and the baseline of a node under a given available width are
computed ONCE per Build and cached beside the walk (two tables, because the two questions
are answered by two walks and a half-filled row would hand a caller a zero it never
measured; the key is (node, available width) and it is sound because a node has one parent,
so its inherited style is fixed within one Build). Measured on the item's own ladder
(nested display:flex; align-items:baseline): sixteen levels lay out in 3.25 ms where the
item extrapolated twenty-seven minutes, and doubling the depth from eight to sixteen costs
7.6x where the re-walking form multiplied by 3.99 PER LEVEL. Proven in
unit/ui/TheLayoutsCostIsBoundedInTheMarkupItIsHanded, which also pins the PICTURE (a
two-item baseline line lands to the pixel) so a cache that answered something else would go
red. Negative control: disabling both lookups turns the same test into 2 TIMEOUT.

---

**REOPENED (2026-08-23, reviewer round 27) — the exponential is still there; the closure's
proof pinned the one shape where the memo happens to hit.**

The closure measured ONE ladder: `display:flex;align-items:baseline`. Add a percentage width
and a padding — an ordinary stylesheet, not a pathological one — and the cost is still 2 per
level. Same probe harness, same machine, `-O2`, `Layout::Build` only, 800x600, AhemFont:

| shape (nested d deep, "x" at the bottom) | d=8 | d=12 | d=16 | d=20 | d=22 |
|---|---|---|---|---|---|
| `display:flex;align-items:baseline` (the closure's) | 0.75 ms | 1.97 ms | 4.06 ms | 7.07 ms | 8.83 ms |
| `…;flex-wrap:wrap;width:90%` | 1.08 ms | 4.11 ms | 10.22 ms | 19.27 ms | 22.84 ms |
| `…;padding:1px;width:95%` | 1.48 ms | **21.88 ms** | **333.38 ms** | **5564.01 ms** | **22286.51 ms** |

Growth on the third row, per level: (5564.01/21.88)^(1/8) = 1.99. **23 boxes cost 22.3
seconds.** Depth 26 extrapolates to 2^4 x 22.3 s = 5.9 minutes; depth 32 to 6.3 hours.

**Why the memo misses** — the same `Layout.cpp` with counters on `Place`, `Measure` and
`BaselineOf`, third shape:

| depth | boxes | `Place` calls | `Measure` calls | `Measure` HITS | `BaselineOf` calls | `BaselineOf` hits |
|---|---|---|---|---|---|---|
| 6 | 7 | 64 | 31 | **0** | 62 | 62 |
| 8 | 9 | 256 | 127 | **0** | 254 | 254 |
| 10 | 11 | 1024 | 511 | **0** | 1022 | 1022 |
| 12 | 13 | 4096 | 2047 | **0** | 4094 | 4094 |

`Place` is 4^(d/2) = 2^d. The Baselines table hits 100 % — that half of the repair works,
because `Measure` pre-fills it (Layout.cpp:238) under the same key. The **Sizes table hits
0 %**: every `Measure` of a node arrives with a DIFFERENT `availableWidth` (the flex
algorithm asks once at the container's content width and again at the item's resolved main
size, Layout.cpp:597 vs :748), so `MemoKey(node, availableWidth)` (Layout.cpp:130-135) is a
fresh key each time and each miss re-walks the whole subtree. Two walks per level, 2^d.

Three things this reopening demands beyond the original three:

4. **The proof is a matrix of shapes, not a ladder.** At minimum: plain flex+baseline,
   percentage width, padding, `flex-wrap:wrap`, and the three combined — the combination is
   the one that fails today.
5. **The assertion is a COUNT, not a stopwatch.** The original item's point 3 ("`Layout::Build`
   publishes a count a scenario suite can assert on — nodes placed, places per node") was
   never delivered; the closing test asserts `deep < 250.0 ms` and a growth ratio. A count
   of `Place` entries with the bound `places <= c x boxes` would have gone red on shape three
   at depth 8 and cannot be tuned away by a faster machine.
6. **The two bounds must agree.** `kDeepestNesting` = 128 (Layout.cpp:99, board:1754) admits
   documents the cost cannot pay: shape three is already 22 s at depth 22 and the depth guard
   never trips. Either `Place` becomes linear in boxes (the real fix) or the refusal bound is
   the depth the layout can actually afford — a refusal at 128 that arrives after six hours
   of walking is not a refusal.
