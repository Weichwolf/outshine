Type: feature
State: open
Parent: 1573
Area: apps
Tags: driver, acceptance, product

# The driver drives at the bar, and the architect signs it off

`apps/driver` is outshine's ONE integration test and simultaneously its product. The library's
other suites are unit tests — each asserting something that CAN be trivially true, checkable by
eye. Emergence is judged here, on the picture, by the hourly architect.

**The day the driver is a driving simulation at Gran Turismo 7's level in an OSM world and the
architect accepts it, outshine's integration test has passed.**

This item holds the ledger, because a ledger is changing content. The architect rewrites the
table each round from what it SAW — `test/run.sh --drive` leaves ten stills evenly along a
declared route — and never from reading the implementation.

## The ledger (2026-08-25, first entry)

| | stands | how it is known |
|---|---|---|
| a program a user runs | yes | the gate builds it and it answers `--help` |
| ten stills along a declared drive | yes | `test/run.sh --drive` wrote them |
| consecutive stills DIFFER — the thing moves | **NO** | every still is the same picture |
| ground under the car | **NO** | white background |
| a horizon, a sky that matches the clock | **NO** | white background |
| shadows, contact or cast | **NO** | nothing to cast onto |
| road markings, guard rails, oncoming carriageway | **NO** | not in any still |
| buildings, trees, water | **NO** | not in any still |

## What will be true

- [ ] Every row above says yes, and the architect writes *ABGENOMMEN* in its report.

## Comments

- 2026-08-25 — opened on the owner's cut: the driver has no tests of its own, everything it uses
  is library, and the architect's acceptance IS the integration result. `apps/driver/test/` — 2080
  lines — was deleted the same hour; what it held that was library work is recorded in board:1862.
