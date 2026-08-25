Type: task
State: open
Area: test
Tags: corpora, invariants

# The invariants test/unit asserted are reclaimed from established corpora

`test/unit/` is deleted: 170 cases that asserted the shape of a moving architecture. The owner's
argument holds — a test specifies only if it stands before the code AND the unit survives, and
neither is true while TARGET moves. What proves the tree is a client that compiles against
`include/outshine/` and renders, plus corpora whose truth does not depend on our design.

**52 of the 170 asserted something INVARIANT** — true whatever the architecture does. Those
claims are not wrong, they were merely housed in the wrong place. This item lists them so they
are reclaimed rather than forgotten, each from an established corpus where one exists
(`test/CORPORA.md` is the survey).

## The invariants that no longer have a claimant

| what was asserted | where an established corpus can assert it |
|---|---|
| a polyline describing a circle of radius R is fitted at R, at every digitisation density | no corpus; the sharpest self-written invariant the tree had, and the one that carried board:1795's repair from 0.667 to 1.000 |
| an arc's laid curvature is 1/R and reverses only where the polyline reverses | geometry/spline suites — see CORPORA.md |
| a refusal names the number it refused on | none; a house rule, provable by reading |
| base64 with and without padding decodes to these bytes | RFC 4648 vectors |
| KHR_xmp_json_ld packets survive with their structured values | Khronos glTF-Sample-Assets, already running |
| two grids that meet share their boundary posts | none; the driver's picture shows it |
| a crossing is counted once whatever the hash does | none; a house rule |
| the speed plan's quantiles describe its own samples | none; a house rule |

## What will be true

- [ ] Every row above is either asserted by an established corpus that runs, or written down in
      `test/CORPORA.md` as a gap with the reason no corpus covers it.
- [ ] The three corpora the survey ranks highest are fetched, prepared and run.

## Comments

- 2026-08-25 — filed at the moment of deletion, so the 52 are a list rather than a memory.
