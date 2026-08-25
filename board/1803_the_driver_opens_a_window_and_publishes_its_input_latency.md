Type: task
State: open
Parent: 1573
Area: apps, clients
Tags: driver, instrument

# The driver opens a window and publishes its input latency

`board:1573`'s M0, and it is first because every criterion the requirement states is a
measurement at a running window:

- the tangent point inside the frame on `hairpin` routes
- the instrument reading trial, +/-5 km/h in <= 0.5 s
- camera stillness, p99 under one pixel of angular change per frame
- input to present, p99 <= 50 ms

None of them is answerable against an offscreen renderer, and today `apps/driver/window/`
renders offscreen: the only `SDL_CreateWindow` in the tree is the browser's, under `tools/`.

## What will be true

- [ ] `apps/driver` opens a real window and drives it with the declared `InputMap` through
      `InputPump` -- the same bindings the scenario declares, no second spelling.
- [ ] A key moves the car, and the case publishes **input to present** as p50/p95/p99, named as
      PIPELINE latency rather than photon latency, because a photon measurement needs a
      high-speed camera and the tree has none (board:1491).
- [ ] Frame pacing is published beside it: p99 of `|dt - 16.67 ms|`.
- [ ] Proving test: the case itself, running inside the runner's budget (board:1778).
      Negative control: the binding removed from the scenario -> the key moves nothing and the
      case says so, rather than the car moving from a hard-wired key.

---

## Sharpened (review 2026-08-24, :17 round) — `apps/` holds no application yet

`CLAUDE.md`'s Setup table, rewritten this session, says what the third tree is for:

> `apps/` | applications built ON the library: what serves the PRODUCT -- **not tests**, but
> they exercise the whole engine and are run by name

At HEAD every file under it is a test:

```
apps/driver/APlannerFindsTheRoadFromMunichToHamburg.cpp     190 lines   Covers("  CHECK
apps/driver/ASecondRouteIsOnlyTwoCoordinates.cpp            129         Covers("  CHECK
apps/driver/TheRoadEdgeIsContinuousWhereSegmentsMeet.cpp    142         Covers("  CHECK
apps/driver/stills/StillsAreTakenAlongTheDriveForTheEye.cpp 768         Covers("  CHECK
apps/driver/window/AWindowShowsTheRoadTheCarIsDriving.cpp   513         Covers("  CHECK
```

Five case sources, no `main` a person runs, no binary with a name a person types. `test/run.sh`
builds them as cases of the suites `apps/driver`, `apps/driver/stills` and `apps/driver/window`,
gives each a PASS/FAIL and counts them in the trailer. The rename moved the directory; it did
not move the KIND.

That is not an argument against the split -- the split is right, and `tools/` (process) against
`apps/` (product) is the correct axis. It is an argument that this item is what makes the
sentence true: **the driver's first artefact must be a program, with the cases beside it rather
than instead of it.** A window that opens, takes the declared bindings and publishes its latency
is an application; a case that opens a window inside a CHECK is still a test.

- [ ] `apps/driver` produces a runnable binary whose entry point is not a case: it reads
      `f31.scenario`, opens the window, and runs until the person closes it.
- [ ] The five cases stay and become proofs ABOUT that program rather than substitutes for it,
      so the `Covers("` exemption in `TheSourceCarriesNoCommentary` keeps meaning what it says
      (see `board:1801`) -- and the program itself, being no proof, carries NO comments.
