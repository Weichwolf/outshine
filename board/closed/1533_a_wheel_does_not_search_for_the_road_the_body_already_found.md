Type: bug
Area: generators
Tags: perf instrument

**A wheel does not search for the road the body already found**

The drive costs **25.6 microseconds a step** -- 25.0 million steps in 639 s for 6.94 simulated hours --
and almost all of it is RESECTION. Every step resects five times: once for the body, and once inside
each of the four `Stand()` calls that ask where the ground is under a wheel. Each resection is a coarse
scan plus 24 golden-section iterations, so a step evaluates the corridor roughly **150 times**.

**The four wheels are rigidly attached to the body.** Their station is the body's station plus a known
arm, and their offset across the corridor is the body's offset plus that arm projected onto the left
normal -- which is already how the CARRIAGEWAY EDGE test works, since the run that found the 96.53 m
index bug. The height and the normal are the only things still going through a search.

- [ ] **A mount's station is derived from the body's**, not searched for, and `Stand()` gains a form
      that takes a station rather than a point
- [ ] **The saving is measured against 25.6 us a step** and published, because a speed-up nobody
      timed is a claim
- [ ] **The drive's answer does not move.** 774.851 km, no wheel off the carriageway, worst deviation
      0.875 m -- a faster run that arrives somewhere else has not been made faster

## Comments

**This is where the time is, and the owner's question about partial PNG decoding is what made it
visible.** Decoding only the rows an elevation tile actually needs is possible ONLY as *stop early*:
PNG's Up, Average and Paeth filters reference the row above, Sub references the byte before, and
DEFLATE cannot be entered in the middle because the LZ77 window is 32 KB of history. So the beginning
can never be skipped.

And it would not matter if it could: **130 tiles at 0.8694 ms is 113 ms of decoding in a 639 s run --
0.018 %.** A perfect partial decoder saves tenths of a second in ten minutes. The resections cost four
orders of magnitude more.

*Measure before you reach* -- and the measurement here says the cheap-looking optimisation is worth
nothing and the boring one is worth 3 to 4 times.

## Closed -- 4.6x, and the answer moved seven millimetres

`Carriageway` gains `StandAt(over, alongM, acrossM, halfWidthM)`, which takes a station instead of
searching for one; `Stand` now resects and calls it, so there is one surface computation and two ways
in. A mount's station is the body's plus the arm projected onto the heading, and its offset is the
body's plus the arm projected onto the left normal -- the same projection the carriageway edge test
already used.

| | before | after |
|---|---|---|
| wall clock for the route | 605 s | **131 s** |
| faster than real time | 41.55x | **191.16x** |
| resections per step | 5 | **1** |

**And the drive arrives in the same place**: 774.852396 km against 774.852403, a worst deviation of
0.876317 m against 0.875457. The arm is projected as a straight where the corridor curves, and over
1.4 m on the tightest radius the route carries that is worth about 5 mm -- which is what the seven
millimetres over 774 km is.

*A faster run that arrives somewhere else has not been made faster*, and this one arrives within a
centimetre of where it did.
