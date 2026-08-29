Type: bug
State: active
Area: render
Tags: measured, places

# DISTANCE is visible on opaque geometry, so the Alps read as the Alps

**Benchmark** — Unreal: `SkyAtmosphere` writes an AERIAL PERSPECTIVE volume — a froxel LUT of
transmittance and in-scattered radiance along the view ray — and every opaque material samples it, so
a distant surface is atmosphere-blended by construction rather than by an authored fog. RAGE: the
timecycle drives a distance fog applied to all geometry, keyed on the hour. **They agree that opaque
geometry takes an atmospheric term by DISTANCE**, so the matter is closed and only the form is mine.
**Taking Unreal's**, because this tree already computes the physics its LUT is made of and a hand-set
fog colour would disagree with the sky standing right next to it.

## What the picture said, and what the numbers said after it

The goal names it: from the Jura, across the Mittelland, you must see the Alps. The frame looked like
a lawn to the horizon. **The first cause I wrote for that was wrong** — the Alps are not missing:

    relief: the ring's tallest vertex ABOVE THE ELLIPSOID   4166 m
    relief: and how far out it lies                       144 580 m
    relief: the ring's lowest vertex                            6 m
    relief: so the true relief, with the sphere taken out    4160 m

4 166 m at 144.6 km is the Alps, in the ring, drawn. And they are drawn at the right SIZE: the drop
over 144.6 km is `d^2/2R` = 1 640 m, so a 4 166 m summit stands about 1 966 m above the eye line at
144.6 km, which is 0.78 deg, which at this lens is about ten pixels. Magnified four times, the ridge
line is there and has peaks.

**They do not READ as the Alps because 145 km of atmosphere costs almost nothing.** Sampled across
the depth of the same frame:

    where                      R      G      B
    sky just above horizon    83.9  112.5  132.2
    far ridge, ~145 km        94.2  111.6   87.6
    mid ground, ~40 km        96.0  113.7   92.3
    near ground, ~2 km        75.9   92.0   74.5

From 2 km to 145 km the blue channel moves 74.5 to 87.6 while the sky sits at 132.2. A ridge behind
145 km of air must approach the sky it stands against; this one stays green. Distance is not visible,
so a 4 000 m range at 145 km is indistinguishable from a 600 m hill at 20 km, and the eye reads the
nearer one.

**AND THE CAPABILITY IS ALREADY HERE, UNREACHED.** `Stage` runs MediumTransmittance,
MediumMultiScatter and MediumRadiance, and `subjectLit.msl` mentions transmittance NOWHERE. The sky
gets the atmosphere and the geometry standing in front of it does not — which is this tree's named
commonest defect, a complete capability no declaration reaches.

## Also found, and it is why this item was nearly filed on a false premise

`Jura 100 tile(s) ... 15304 m relief` was the number that made the horizon look empty of data. Over a
395 km reach the Earth alone drops `395000^2 / (2 * 6371008.8)` = **12 244 m**, so that figure is
73 per cent curvature. It reads like a mountain range on a ring with NO elevation data at all. The
instrument now publishes height above the ELLIPSOID, where 4 160 m is the answer and is checkable.

## The measurements that would show I am wrong

1. **The far ridge's blue must rise toward the sky's while the NEAR ground does not move.** If the
   near ground shifts too, the term is a global tint and not a distance term
2. **The negative control is a zero-density atmosphere.** With the medium's density set to nothing,
   the far ridge must return to exactly today's numbers. A fog that survives an atmosphere being
   switched off is an authored fog wearing a physical name
3. **The sky must not move at all.** It already samples the medium; if it changes, the new term is
   being applied twice
4. **Cost, as a BOUND and not a tick**: `apps/bench` before and after. A froxel volume is a compute
   pass and a 3D sample per fragment, and the frame budget is what it is
