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

Measured at c0de1b18, at a32c4919 and AGAIN, unchanged, at **84115df7** -- a THIRD review round
began with `mkdir`, and this is the only item standing between the acceptance command and a
picture. At 84115df7 the declared drive finally ROUTES (board:1862, 1894, 1911), so the run now
gets all the way to the capture before losing it:

    ROUTED the declared drive
    REFUSED the screenshot could not be opened for writing at DIR/along01.png
    exit 1, DIR not created, nine stills lost

With `mkdir -p DIR` first, the same binary and the same command:

    DROVE 15531 frames over 2.895 of 2.915 km, kept 9 still(s)

Nine, not ten -- the second half of this item, unchanged for three rounds.
`engine.Capture(named)` fails because nobody created the directory. The refusal is printed and
the exit code says so; what is missing is the `mkdir`, one line, before the drive starts.

Two more counting defects in the same loop:

| asked | kept | why |
|---|---|---|
| `--stills 10` (default) | 9, at three HEADs running | the tenth needs `alongM >= routeM` (main.cpp:196-198) and the drive stops at 2.895 of 2.915 km, inside its arrival tolerance |
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
