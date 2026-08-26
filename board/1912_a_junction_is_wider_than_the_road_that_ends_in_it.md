Type: bug
State: open
Parent: 1499
Area: sim, actor/path
Tags: measured, geometry, drive

# The accuracy bound at a corner is the junction's width, not one road's half width

`CorridorLay` hands `Fit` ONE accuracy bound for the whole route, and it is the widest half
width on it:

    src/sim/CorridorLay.cpp:65   if (half > roadWithinM) { roadWithinM = half; }
    src/sim/CorridorLay.cpp:76   fitted = Fit(keptM, roadWithinM, tightestM, classTightestM, corridor);

On the shipped Munich network that is **3.75 m**, a 7.5 m road. It refuses a normal street
corner:

    REFUSED vertices 9..9 turn through 1.914658 rad and the widest arc that stays within
    3.750000 m of them is 4.668083 m, tighter than the 4.901673 m this vehicle can bend to

## The arithmetic, and it is 0.23 m short

    swing      = 1.9147 rad = 109.7 deg        half = 0.9573,  cos = 0.5764
    ShiftShare = 1 + swing^2/96 = 1.0382
    byAccuracy = withinM / (ShiftShare/cos(half) - 1) = 3.75 / 0.8011 = 4.68 m
    the F31 bends to                                                   4.90 m

**A wide arc at a sharp corner cuts FAR from the corner.** Holding it within half a road width
forces a radius tighter than any car's lock, so every sharp junction on every route is a refusal
waiting to happen -- and 109.7 degrees is an ordinary city corner, not a hairpin.

## Why the bound is wrong there, physically

A car turning 110 degrees does not stay within half a lane of the corner point. It uses the
JUNCTION, and a junction is not a road: it is at least as wide as the two ways that meet in it.
The bound at a corner is therefore the SUM of the two half widths, not the larger of them --
derived from the ways, not chosen:

    two 7.5 m roads meeting:  withinM = 7.5 m  ->  byAccuracy = 9.36 m,  drivable

## What will be true

- [ ] The accuracy bound is per VERTEX, the way `classTightestM` already is: a corner between two
      ways is bounded by the junction they form, a point along one way by that way's half width.
- [ ] Proving case: a 110-degree corner between two 7.5 m ways fits at a radius the declared
      vehicle can drive, and the same corner between two 3 m ways still refuses by name. Negative
      control: one bound for the whole route, and the first case refuses.
- [ ] The refusal keeps naming the number when it does refuse -- a corner tighter than the lock
      after the junction is accounted for IS a finding about the graph, and board:1911's new
      junctions are where to look first.
