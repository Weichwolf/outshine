Type: bug
State: open
Parent: 1499
Area: actor, sim
Tags: measured, geometry, alignment, corner

# A hairpin is bounded by something, and the fit optimises what it accepts

board:1912 put the accuracy bound per vertex and derived the corner's bound from the kerb. The
kerb formula is right and I checked it: for half widths wA, wB meeting at deflection D the inner
offsets cross at `sqrt(wA^2 + wB^2 - 2 wA wB cos D)/sin D`, which is `w/cos(D/2)` when the widths
agree. Three things around it are not.

## 1. THE BOUND IS UNBOUNDED WHERE THE FIT IS MOST LIKELY TO BE WRONG

`src/sim/CorridorLay.cpp:87` applies it at every interior vertex whose turn is not exactly 0 or
exactly pi. `w/cos(D/2)` diverges as D approaches pi, so the licence a hairpin gets is arbitrary:

    half width 3.75 m       5 deg    3.75 m
                           90 deg    5.30 m
                          109.7 deg  6.51 m      <- the corner board:1912 measured
                          150 deg   14.49 m
                          170 deg   43.03 m
                          178 deg  214.87 m

A switchback, a loop ramp or a U-turn at a roundabout approach is a 150-175 degree deflection in
ordinary OSM data, and at 170 degrees the built corridor may leave the polyline by 43 m and be
accepted as "still inside the junction". The guard at `CorridorLay.cpp:83` excludes only the exact
limit, so nothing catches the neighbourhood of it. A junction is a bounded piece of made ground
and the bound is a LENGTH; the formula is the crossing of two lines, which is not the same thing
once the lines are nearly antiparallel.

## 2. THE SEARCH MINIMISES A DISTANCE AND THE TEST READS A SHARE

    src/actor/path/Alignment.cpp:158   FurthestFromArcM(...) < FurthestFromArcM(...)      <- objective, metres
    src/actor/path/Alignment.cpp:241   if (held->AwayShare <= 1.0 || last == at)          <- acceptance, share

The ternary search over the multi-vertex run picks the radius that minimises the worst DISTANCE
from the polyline; the run is then accepted on the worst SHARE of each vertex's own allowance.
The two agree only while every allowance is equal, which is exactly the case the per-vertex span
was introduced to leave. So the arc chosen is not the widest one that satisfies the rule, and a
run that could be carried by one arc splits: `SplitByAccuracy` rises and a transition is inserted
where curvature does not reverse -- against CLAUDE.md's own alignment rule, *one arc per RUN of
same-sign turns*. The single-vertex branch (`:148`) already reads the vertex's own allowance and
is right.

## 3. TWO STATEMENTS OF THE DERIVATION ARE INVERTED

`test/harness/outshine/physics/ScoreWhereACornerFits.cpp:26` and the closing commit of board:1912:

> at D -> 0 it grows without bound, because two nearly parallel roads overlap forever, and at
> D -> pi it falls to w

The code's D is the DEFLECTION (`CorridorLay.cpp:81`, `atan2` out minus `atan2` in), and both
limits are the other way round: D -> 0 gives w, D -> pi diverges. The sentence also contradicts
the closed form printed two lines above it, `w/cos(D/2)`. An `outshine/` oracle is a law we wrote
ourselves and CLAUDE.md asks it to carry its derivation *because the derivation is the part a
reader can check* -- this one fails that check, and it hides defect 1 from the reader who makes it.

## What will be true

- [ ] The corner bound is finite at every deflection a road can have, and the number that bounds
      it is named and derived. Proving case: the same corner walked from 5 to 179 degrees, the
      bound monotone and bounded, refusing where the junction cannot be built.
- [ ] `Align` optimises the quantity it accepts. Negative control: unequal allowances across one
      run, and the metric objective picks a radius the share test rejects while a wider one passes.
- [ ] The refusal at `Alignment.cpp:171` names the allowance that decided, not the global
      `withinM` -- today it prints 1.5 m where 2.61 m was applied.
- [ ] The derivation beside the number states the limits the code computes.
