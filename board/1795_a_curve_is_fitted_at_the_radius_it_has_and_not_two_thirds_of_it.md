Type: bug
State: active
Parent: 1499
Area: actor/path
Tags: drive, geometry, measured
Supersedes: 1784

# One arc per RUN of same-sign turns, at the radius the line carries

`Fit` puts a spiral-arc-spiral at EVERY vertex and returns the curvature to zero between them.
For a smooth curve digitised into short chords the room bound collapses to

```
TangentShare(s) -> 0.75 s          src/actor/path/Fit.cpp:12-14
byRoom          -> 0.5 L / 0.75 s = (2/3) * (L/s)        Fit.cpp:82
```

while the arc the polyline describes has `R = L / (2 sin(s/2)) -> L/s`. **The fit lays exactly
two thirds of the radius the line carries, at every digitisation density** — reproduced to four
digits over five values of alpha and over chord lengths from 10 m to 400 m. No constant repairs
it: a transition is owed where the CURVATURE changes, not where a digitiser put a point.

The consequence on the shipped route: curvature reverses sign three times in 45 m and reaches a
5.6 m radius — tighter than the F31's own 11.3 m turning circle — in the middle of a
long-distance route, and the plan crawls 3.2 km of Munich--Hamburg under 30 km/h. The speed
profile is not the defect; `sqrt(HoldingMs2/kappa)` is exactly the number printed. **The profile
obeys a line that no road has.** On that route 769 of 2202 corners (35 %) sit inside a run of
same-sign turns and would be lifted from `R/1.5` to `R*cos(s/2)`.

## What will be true

- [ ] One arc per RUN of same-sign turns; a transition only where the curvature REVERSES.
- [ ] The fitted radius is bounded below by the DESIGN MINIMUM the road class carries, and a
      geometry that cannot hold it refuses by name instead of planning 12 km/h on a trunk road.
- [ ] The split rule at a run's end — when the accuracy bound forces a run apart — is decided
      and argued, which is why TARGET's `Alignment` is amber.
- [ ] Proving test: a true circular arc of R = 400 m fitted at any density reproduces 400 m to
      the accuracy bound; negative control — the per-vertex fit restored, the case reads 266 m.
