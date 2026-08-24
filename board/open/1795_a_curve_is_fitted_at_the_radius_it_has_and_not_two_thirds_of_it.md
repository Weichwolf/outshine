Type: bug
Parent: 1499
Area: actor/path
Tags: drive, geometry, measured

# A curve is fitted at the radius it has, and not at two thirds of it

`Fit` reconstructs a polyline as straights with a spiral-arc-spiral corner at each vertex. The
radius it may use at a vertex is bounded by the room between the two legs
(`src/actor/path/Fit.cpp:82`):

```cpp
const double byRoom = 0.5 * shorterLegM / TangentShare(swing);
TangentShare(s) = ShiftShare(s) * tan(0.5 * s) + 0.25 * s        // Fit.cpp:12-14
```

For a smooth curve digitised into short chords the swing per vertex is small, and there

```
TangentShare(s) -> 1 * (s/2) + 0.25 s = 0.75 s
byRoom          -> 0.5 L / (0.75 s) = (2/3) * (L/s)
```

while the arc the polyline actually describes has `R = L / (2 sin(s/2)) -> L/s`. **The fit lays
two thirds of the radius the line carries, and it does so at every digitisation density.**

Computed against the tree's own `CornerRadiusM`, a true circular arc of R = 400 m fitted within
8 m:

| chord L [m] | turn/vertex [deg] | byAccuracy [m] | byRoom [m] | fitted R [m] | of the truth |
|---|---|---|---|---|---|
| 10 | 1.432 | 94 512 | 266.6 | **266.6** | 0.667 |
| 20 | 2.865 | 23 620 | 266.6 | **266.6** | 0.666 |
| 50 | 7.167 | 3 770 | 266.2 | **266.2** | 0.666 |
| 100 | 14.362 | 934 | 264.9 | **264.9** | 0.662 |
| 200 | 28.955 | 225 | 259.6 | 225.1 | 0.563 |
| 400 | 60.000 | 47.7 | 236.5 | 47.7 | 0.119 |

Two thirds, to three digits, independent of how finely the curve is drawn. It is not sampling
noise and it is not the graph: it is `0.25 * s`, the clothoid's own tangent contribution,
sitting in a bound that is then applied as if the corner stood alone between two straights. A
dense arc gives every vertex a corner, the corners consume the whole leg, no straight is left,
and the laid line's curvature band oscillates between zero and `1/266` where the road's is a
flat `1/400`.

The heading is right -- each corner turns by exactly its own swing, so the line arrives where
the polyline does. **The PEAK curvature is 1.5x too high**, and peak curvature is precisely what
`SpeedProfile` bounds speed by (`sqrt(HoldingMs2 / kappa)`). This is board:1784's original
symptom with its cause named:

> *8 710 samples under 30 km/h ... curvature reverses sign three times in 45 m and reaches a
> 5.6 m radius*

Measured on the shipped Munich--Hamburg route at HEAD, with the class minimum now threaded
(board:1784):

```
NOTE vertices the route offered                                   2204
NOTE legs of the route whose road class declares one       2480 of 2480
NOTE corners the fit laid tighter than their class allows          947
NOTE the sharpest turn it carried                          137.281 deg
NOTE turns past a right angle                                24 of 2480
```

**947 of 2204 corners under their class minimum, with only 24 turns past a right angle on the
whole route.** Sharp turns cannot explain a 43 % violation rate; a systematic factor of 2/3 can,
and does: a genuine 400 m primary curve is laid at 267 m and counted as a violation of the very
class it obeys.

## Consequence for board:1784

The class minimum is now measured and must NOT yet refuse. Refusing on a radius that is a third
low would refuse roads that obey their class -- the instrument has to be trustworthy before it
is allowed to say no. That is why 1784's first box stays open with a named reason rather than
being closed with a refusal that would break every route in the network.

## What will be true

- [ ] Consecutive vertices whose turns share a sign and whose implied radii agree are fitted as
      ONE arc, so a polyline that describes a circle is reconstructed as that circle.
- [ ] Proving test: `test/unit/actor/path/` builds a true circular arc of a known radius at
      several chord lengths and requires `Fitted::TightestRadiusM` to be that radius within a
      stated bound. Negative control: the merge removed -> 0.667 of it, the table above.
- [ ] The Munich--Hamburg count of corners under their class minimum is re-measured and
      published beside the old one; whatever remains is the graph's own and is a finding about
      OSM rather than about the fit.
- [ ] `SpeedProfile`'s crawl on that route is re-measured. board:1784 recorded 8 710 samples
      under 30 km/h over 2.05 million; the number after this repair is what says whether the
      crawl was the fit or the data.
