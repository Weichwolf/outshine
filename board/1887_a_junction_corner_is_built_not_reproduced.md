Type: bug
State: open
Parent: 1499
Area: actor/path, sim
Tags: drive, geometry, measured, driver

# A junction corner is BUILT, not reproduced, and the tolerance that decides it is a road tolerance

The alignment is fitted against the SOURCE's digitisation quantum:

```
src/sim/CorridorLay.cpp:70    fitted = Fit(keptM, quantumM, tightestM, classTightestM, corridor);
src/sim/DriveAssembly.cpp:161 say.Number("the tile's own coordinate quantisation", quantumM, "m");
```

`quantumM` is how precisely the vector tile RECORDS a point. It is being used as `withinM` — how
far the built road may DEPART from the shape the data implies. Those are two different
quantities and one is standing in for the other.

The consequence is measured, 2026-08-25 at 235e3f47, on a 400 m drive inside Munich
(`--from 48.13720,11.57560 --to 48.13500,11.57200`):

```
REFUSED vertices 4..4 turn through 1.595273 rad and the widest arc that stays within 0.597164 m
of them is 1.271100 m, tighter than the 4.901673 m this vehicle can bend to -- a corner tighter
than the lock is a route that doubles back on itself, and that is a finding about the graph
```

1.5953 rad is **91.4 degrees: a street corner.** `src/actor/path/Alignment.cpp:101` computes
`byAccuracy = withinM / (ShiftShare(swing)/cos(half) - 1.0)` = 0.597164 / 0.46927 = 1.2724 m, and
the bound is what refuses, not the vehicle. The refusal text then asserts a cause it has not
measured: a 91-degree corner is not "a route that doubles back on itself" and it is not "a
finding about the graph" — it is what every urban junction looks like in OSM, where the corner is
TOPOLOGICAL and the real kerb carries a 6-12 m radius the centreline never spells.

CLAUDE.md decides this: *infrastructure built from OSM is PLAUSIBLE and geometrically correct,
never necessarily true to the real road; the data is a source of shape, not a specification to be
reproduced.* A junction is exactly where the built road is ENTITLED to leave the polyline. At
R = 8 m the arc departs the corner by `R*(1/cos(half) - 1)` = 3.45 m, and that is a correct road.

This is not board:1795. That item is the radius a fit lays inside a RUN of same-sign turns; this
one is which tolerance feeds the fit, and what a corner between two straights is allowed to be.

## What will be true

- [ ] The fit takes a ROAD tolerance, named as such and carrying its origin, not the tile's
      coordinate quantum. The quantum bounds where a POINT is; the tolerance bounds where the
      ROAD may be built, and at a junction it is metres.
- [ ] A corner between two ways at a junction is built to the kerb radius its classes imply, and
      the vehicle path is the one the corner permits — never a refusal.
- [ ] A refusal that remains says only what it measured. "The graph doubles back" is a claim
      about the DATA and may be made only when the turn exceeds what any road geometry could
      resolve, not when a tolerance the engine chose was the binding term.
- [ ] Proving case: a scenario over a right-angled urban junction lays a corridor whose minimum
      radius is at least the class kerb radius and whose maximum departure from the OSM
      centreline is reported; negative control — the tolerance set back to the tile quantum and
      the case refuses by name.
