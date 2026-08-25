Type: bug
State: open
Parent: 1865
Area: apps
Tags: driver, acceptance, measured

# The acceptance run leaves its stills, or says out loud why it did not

The architect's command is fixed and it is the one an owner types:

```
build/outshine-driver --headless --into DIR --assets .../apps-driver-f31
```

Measured 2026-08-25 at c0de1b18: with `DIR` not already present, the run prints its eighty
measured lines, prints `NO DRIVE -- the picture is what stood without it`, exits 1 and leaves
**nothing at all** -- no directory, no file, no message. `engine.Capture(named)` fails because
nobody created the directory, and `apps/driver/src/main.cpp:171-173` prints only when it
SUCCEEDS, so the one path the tree calls *a failure is loud* is the one path that is silent.
`mkdir -p DIR` first and the same command keeps `refused.png`.

Two more counting defects in the same loop, both measured on the overridden 302 m drive:

| asked | kept | why |
|---|---|---|
| `--stills 10` (default) | 9 | the tenth needs `alongM >= routeM` (main.cpp:196-198) and the drive stops at 282 m inside its arrival tolerance |
| `--stills 1` | **0** | with `Stills = 1` the only threshold IS the whole route, so one still is no stills |

A programme that keeps `N-1` of `N` stills is counting the wrong ends: ten stills evenly along a
drive are the fractions `k/N` for `k = 0..N-1`, which puts the FIRST at the start and needs no
arrival.

## What will be true

- [ ] `--into DIR` creates `DIR`, or refuses by name before the drive starts. A capture that
      fails prints what failed and the run's exit code says so.
- [ ] `--stills N` keeps exactly N for any `N >= 1`, spaced at `k/N`, the first at the start.
- [ ] Proving case: the acceptance command into a fresh path leaves one file per still and the
      architect's round never begins with `mkdir`.
