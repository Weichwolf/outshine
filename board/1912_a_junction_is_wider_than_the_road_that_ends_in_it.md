Type: bug
State: open
Parent: 1499
Area: sim, actor/path
Tags: measured, geometry, drive, review

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

## The proposed number is a CHOICE and the item owes a derivation

**Reviewed 84115df7: the sum of the two half widths is asserted, not derived.** It is the right
direction and the wrong kind of answer — it replaces one arbitrary bound with another that
happens to be twice as large, and CLAUDE.md says every number carries its origin.

What bounds an arc at a junction is the KERB, and the kerb is constructible from the two ways
alone. `withinM` bounds the arc's EXTERNAL distance from the PI (`byAccuracy = withinM /
(ShiftShare/cos(D/2) - 1)` is the radius whose external distance is `withinM`), so the bound is
the distance from the PI to the inner kerb corner. Two centrelines of half widths wA and wB
meeting at interior angle **t = pi - D**, each offset inward by its own half width:

    E_max = sqrt(wA^2 + wB^2 + 2 wA wB cos t) / sin t
    and for wA = wB = w it reduces to     w / sin(t/2)  =  w / cos(D/2)

For the measured corner — two 7.5 m ways, D = 1.9147 rad, t = 70.3 deg, w = 3.75 m:

    E_max      = 3.75 / cos(54.85 deg)      = 6.51 m
    byAccuracy = 6.51 / 0.8011              = 8.13 m     the F31 bends to 4.90 m -> drivable
    the sum rule would give 7.50 m -> 9.36 m, 15 % wider, and from nowhere

The kerb form is the one to write: it is exact, it takes UNEQUAL widths without a second rule,
and it degenerates correctly — as D goes to 0 the bound goes to infinity because a straight
needs no room, and as D goes to pi it goes to w because a U-turn has only its own carriageway.

## Why the bound is wrong there, physically

A car turning 110 degrees does not stay within half a lane of the corner point. It uses the
JUNCTION, and a junction is not a road -- it is the area the two kerb lines enclose. One bound
for the whole route cannot know that, because it does not know which vertex is a corner between
two ways and which is a bend along one.

## What will be true

- [ ] The accuracy bound is per VERTEX, the way `classTightestM` already is: a corner between two
      ways is bounded by the kerb corner they form, a point along one way by that way's half
      width. The number prints with its origin and its population.
- [ ] Proving case: a 110-degree corner between two 7.5 m ways fits at a radius the declared
      vehicle can drive, and the same corner between two 3 m ways still refuses by name. Negative
      control: one bound for the whole route, and the first case refuses.
- [ ] The refusal keeps naming the number when it does refuse -- a corner tighter than the lock
      after the junction is accounted for IS a finding about the graph, and board:1911's new
      at-grade junctions are where to look first.
