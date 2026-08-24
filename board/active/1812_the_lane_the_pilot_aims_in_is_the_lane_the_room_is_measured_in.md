Type: bug
Parent: 1767
Area: sim
Tags: two-truths, measured, drive

# The lane the pilot aims in is the lane the room is measured in

Found by attributing `board:1767`'s wheel-off-the-carriageway at km 113.990. The two numbers
that decide whether a wheel is on the road are read at **different stations**.

`src/sim/CorridorLay.cpp:310-314` bakes the fine arrays from a coarse post:

```cpp
for (size_t fine = 0; fine < fineAside.size(); ++fine) {
  const size_t post = (size_t)((double)fine * fineM / spanM);
  fineAside[fine] = asideM[band];
  fineEdge[fine] = halfWidthM[band];
}
```

`src/sim/DriveTick.cpp:66` reads the aim at the FINE index and `:156, :199` read the lane at the
COARSE one:

```cpp
const size_t fine = (size_t)(at.AlongM / fineM);
const double wantAsideM = fineAside[fine];              // aim: baked at floor(fine*fineM/spanM)
...
const size_t post = (size_t)(at.AlongM / spanM);
laneHalfM[post]                                          // room: read at floor(AlongM/spanM)
```

`fine * fineM <= at.AlongM`, so the baked band is `floor(fine*fineM/spanM)` and the read post is
`floor(at.AlongM/spanM)`. **They differ by one whenever a station boundary falls inside a fine
step** -- which is every post boundary, because `fineM < spanM`.

## Measured at the crossing

```
NOTE the lane it was in                = 3.750000 m      -> its centre is -1.875 m
NOTE what the pilot was aiming for     = -2.125000 m     -> that is a 4.25 m lane's centre
NOTE how far the aim still had to travel = 0 m           -> the aim was NOT mid-move
```

`asideM[post] = -0.5 * (lanes - 1) * laneM` with `laneHalfM[post] = 0.5 * laneM` are written in
ONE loop (`CorridorLay.cpp:270-282`) from one `halfWidthM[post]` and one `lanes`, so they cannot
disagree **at the same post**. The aim says 4.25 m and the room says 3.75 m, and the rate
limiter had finished, so the two were read one station apart.

The consequence is exactly `board:1767`'s failure: the pilot holds its car at the centre of a
lane that is 0.25 m wider than the one the room is measured in, and 0.25 m is a quarter of the
0.97 m the lane leaves either side of a 1.811 m car.

## What will be true

- [ ] The aim, the corridor edge and the lane half-width are read at ONE station -- one array
      family at one resolution, or all three baked fine from the same band.
- [ ] Proving test: a synthetic corridor whose lane width steps, driven across the step, in
      which the aim and the room agree at every station. Negative control: the two indices put
      back -- the aim belongs to one lane and the room to its neighbour, and the test names the
      station.
- [ ] `board:1767` is re-measured against the repair: the deviation at km 113.990 was 0.888 m
      of a 0.970 m margin, and 0.25 m of it is this.

---

## The reading above is WITHDRAWN, and the mechanism is worse (2026-08-24)

The index mismatch is real and it is repaired -- `FineLaneHalfM` is baked from the same band as
`FineAside` and `FineEdge`, and the tick reads all three at the fine index, so no two of them
can name different stations. **It is not what happened at km 113.990.** Measured directly out
of the corridor's own arrays:

```
NOTE fine index    = 56995        coarse index = 1181
NOTE FineAside     = -2.125 m     AsideM at the coarse post     = -1.875 m
NOTE FineLaneHalfM =  1.875 m     LaneHalfM at the coarse post  =  1.875 m
NOTE FineEdge      =  3.750 m     HalfWidthM at the coarse post =  3.750 m
```

Same band -- `floor(56995 * 2 / 96.5185) = 1181` -- and the fine AIM alone disagrees with its
coarse original. Something rewrites it after the bake, and `CorridorLay.cpp:330-390` is what:
`fineAside` is clamped to a room and then rate-limit smoothed **bidirectionally over 400
sweeps**, so an aim is carried across a width change rather than stepping. That is deliberate
and right -- a car cannot jump 0.25 m sideways.

## What is actually wrong: a reserve nobody honours

The clamp the smoothing is held to is

```
room = fineEdge - 0.5 * carWidth - budgetM = 3.75 - 0.9055 - 0.7195 = 2.1250
```

and the aim sits at **exactly** that: -2.125. So the aim is not a lane centre at all there --
it is the corridor's own outer limit, and `budgetM` is what the corridor reserves between the
aim and the edge for the car's tracking error.

```
budgetM               = 0.5 * narrowestLane - 0.5 * carWidth = 0.7195 m
the drive's own error = 0.8884 m
over by                                                        0.1689 m
```

**The corridor reserves 0.7195 m for tracking error and the drive produces 0.8884 m.** The
wheel crosses by 0.169 m, which is that overrun to the millimetre.

And `budgetM` is not a tracking allowance derived from anything the car does: it is *all* the
room the narrowest lane on the whole route leaves a 1.811 m car. It is a width, used as an
error budget, applied everywhere.

## The two repairs, and the measurement that chooses

| | |
|---|---|
| **the pilot tracks within the budget** | the deviation is 0.888 m where 0.720 is reserved. `board:1767` measured the pilot commanding 2.52x the kinematic steer with tyres at 0.33 deg of a 3.91 deg peak, so it is neither under-commanding nor sliding -- the convergence is simply slower than the corridor assumes |
| **the budget is what the pilot achieves** | then it is a MEASURED number with a population, not `0.5 * narrowestLane - 0.5 * carWidth`, and the corridor refuses a road it cannot hold the car in rather than laying one and driving off it |

The second is the honest shape and the first is the better car. **Neither may be chosen from
this station alone**: one drive's worst deviation is one sample, and a budget fitted to it is
the calibration-decides defect this tree forbids. What the next round needs is the deviation's
DISTRIBUTION over the route -- p50/p95/p99, the way every other number on that page is
published -- and the item is parked there rather than guessed at.
