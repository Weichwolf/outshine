Type: feature
State: open
Area: ui
Tags: scope, performance, driver
Supersedes: 1753

# The UI updates a number without reparsing the world, and its layout is bounded

The markup/style/layout tree is the right declarative game UI (flexbox, specificity, UA sheet,
hit-testing, WPT-scored) and two things stand between it and a HUD at 60 Hz.

**The reparse.** `Live::Compose` re-parses the markup and every stylesheet from strings on each
redeclare, so a speed readout costs a full reparse per frame.

**The layout's cost is exponential in nesting depth, not linear in boxes.** `Placer::Place`
re-walks each child's whole subtree up to FOUR times before placing it — `Measure`
(src/ui/Layout.cpp:690), `BaselineOf` (:745), `BaselineOf` again on the same subtree (:799),
then `Place` (:815) — and nothing memoises: `MinContent` (:187), `MaxContent` (:236),
`BaselineOf` (:348) and `Measure` (:169) each recompute from scratch. Measured on a baseline-
aligned flex tree, one text run at the bottom, -O2:

| depth | boxes | `Layout::Build` | ratio |
|---|---|---|---|
| 8 | 8 | 26.4 ms | 4.11 |
| 10 | 10 | 375.9 ms | 3.14 |
| 12 | 12 | 6464.2 ms | 4.07 |

## What will be true

- [ ] A text node mutates and only its subtree relays out; the reparse dies.
- [ ] Intrinsic sizes and baselines are memoised per node per pass, so the cost is linear in
      boxes and the ratio above is 1.0 per added level.
- [ ] Script binds to the tree — events in, node mutation out — still declarative at the seam.
- [ ] The driver's HUD (speed, route) is declared markup, updated at 60 Hz, MEASURED.
