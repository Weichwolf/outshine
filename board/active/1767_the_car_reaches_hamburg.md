Type: bug
Area: actor/path, sim
Tags: drive, speed-profile, grip, goal

# The car reaches Hamburg

`tools/driver/APlannerFindsTheRoadFromMunichToHamburg` is the goal's own proof, and it is RED
at HEAD -- 39 of 41 checks pass, and the two that fail are the two that matter:

```
FAIL ...:148  and no wheel ever left the carriageway, which is the road's declared width
              CHECK(rode.LeftTheRoadAtM <= 0.0)
FAIL ...:152  **THE F31 DROVE ITSELF FROM MARIENPLATZ TO RATHAUSMARKT.**
              CHECK(rode.Arrived)
```

This is NOT a fetch failure. The case fetched: 26 stations, 5 tiles across each, a corridor
of 753.617 km fitted through live OSM ways. It drove, and then it crashed.

**Measured, this run** (one run, warm, this machine):

| | |
|---|---|
| corridor laid | 753.617 km |
| distance driven | **113.990 km** (15.1 % of it) |
| fastest | **225.776 km/h** |
| worst share of a contact's grip used | **1.480 of it** |
| most mounts off the ground at once | **2 of 4** |
| where the contact first went past its limit | 113.990 km |
| where a wheel first left the carriageway | 113.990 km |
| worst deviation from its own lane | -0.887 m, at 113.990 km |

Every one of those "where" numbers is the SAME station. One event at km 113.990 lifts two
wheels, overruns the grip by 48 %, and puts the car off the road; the drive ends there.

225.776 km/h is the F31's drag-limited top speed, so the speed profile planned NO limit at
that station -- it believed the road was straight, level and flat there.

## What the profile does bound, and therefore what it did not see

`SpeedProfile::Over` (src/actor/path/SpeedProfile.cpp:47-95) bounds the plan five ways, all
sampled AT stations of `stepM`:

| term | bound |
|---|---|
| curvature | `sqrt(lateral / curvature)` |
| cornering slip | `cbrt(4 C w / (m k s))` |
| curvature rate | `cbrt(6 w / (rate s^3))` |
| climb | `sqrt((drive - m g slope) / drag)` |
| **crest** | `sqrt(g / -slopeRate)` -- the term that keeps the wheels down |

Two wheels left the ground, so the CREST term is the one that failed. Either
`Placed::SlopeRatePerM` is wrong at that station, or the crest is between two stations and
the sampling stepped over it -- a point sample of a rate cannot see a break it lands either
side of.

## What will be true

- [ ] The station at 113.990 km is measured: its slope, slope rate, curvature and the
      elevation samples the corridor was fitted through, published as numbers
- [ ] Whichever of the two causes it is, the profile bounds it: a term sampled at stations
      must be an INTERVAL statement, not a point one, or the interval it steps over is a
      term it does not have
- [ ] `rode.Arrived` and `rode.LeftTheRoadAtM <= 0.0` both hold over the full 753.617 km
- [ ] A unit case in `test/unit/actor/path/` reproduces the failure WITHOUT the network: a
      reference line carrying the same slope break, and a profile that must bound it

## Comments

- 2026-08-24 -- found while closing board:1554, which ran `test/run.sh tools` to prove it had
  not made things worse. This failure is older than that change and was never filed: `tools`
  is named-only, so the goal's own headline proof is red in a suite the gate does not run.
  board:1766 repaired the compile-time half of that blindness; this is the run-time half.
- `board:1539` asks for this drive to move OUT of `tools/` and into
  `test/render/outshine/drive/` as the single proof. That move is worth nothing while the
  drive itself is broken, and it would carry a red case into a second suite. This item comes
  first.

---

## Reproduced without the network, and repaired (2026-08-24)

The crest term was a POINT statement about a rate. `SpeedProfile::Over` sampled at stations
of `stepM`; elevation knots come from the DEM along the route and owe that step nothing, so a
crest whose knots fall between two stations was invisible to every station on both sides.

**Reproduced with no network at all** -- `test/unit/actor/path/ACrestBetweenTwoStationsIsStillACrest`:
a straight 400 m road, a 10 m crest at 205 m, a 20 m step so the stations are 200 and 220 and
the crest lives entirely between them.

| | before | after |
|---|---|---|
| sharpest crest the line carries | 0.48 per m | 0.48 per m |
| fastest a wheel stays down over it | 16.272 km/h | 16.272 km/h |
| **what the plan allowed** | **259.960 km/h** | **16.272 km/h** |

A factor of sixteen, and the same shape the live drive showed: a plan of 225.776 km/h where
the wheels leave at a fraction of it.

### The repair, and the assumption it stands on

`ReferenceLine::Seams()` publishes the stations where the line's second derivative may jump:
every segment boundary, every rise knot, every bank knot, and the end. `SpeedProfile::Over`
now bounds each SEAM INTERVAL as well as each station, evaluating the terms at the interval's
head, middle and tail, and applying the tightest of the three to every station the interval
touches.

The tail is not sampled -- it is EXTRAPOLATED, `tail = 2 x middle - head`, and that is exact
rather than approximate for exactly the three quantities it is applied to:

| quantity | why the extrapolation is exact |
|---|---|
| `SlopeRatePerM` | the rise is a cubic Hermite, so its second derivative is LINEAR in t: `bend = (12t-6)/span^2 * v0 + (6t-4)/span * m0 + (-12t+6)/span^2 * v1 + (6t-2)/span * m1` (ReferenceLine.cpp:120-122) -- every term linear |
| `CurvaturePerM` | `EntryCurvature + rate * byM` (ReferenceLine.cpp:124) -- linear in the distance along the segment |
| `CurvatureRatePerM` | `(ExitCurvature - EntryCurvature) / LengthM` -- constant within a segment |

`Slope` is deliberately NOT extrapolated: it is the FIRST derivative of a cubic, so quadratic
in t, and a linear extrapolation of it would be a guess. It is inherited from the middle
sample instead.

**That leaves the climb term a POINT statement, and this entry does not pretend otherwise.**
Slope's extremum can fall strictly between two stations, so the tractive bound at
`SpeedProfile.cpp:82-88` can still be evaluated where the slope is not worst. It is a
different KIND of miss from the crest: a crest missed lifts the wheels off the road, a climb
missed only means the car does not reach a speed the plan allowed. The first is a safety
bound and is now an interval statement; the second is a performance bound and is still a
point one. Filed rather than argued away -- board:1773 carries it.

### A second defect, found by the first

`ReferenceLine::Read` answered a bend of **zero** at both ends of the line:

```cpp
if (alongM <= through.front().AlongM) { rate = ...; return; }   // bend left at 0
if (alongM >= through.back().AlongM)  { rate = ...; return; }   // bend left at 0
```

Those guards are for a station OUTSIDE the knots; `<=` and `>=` made them swallow the first
and last knot too, so a crest at either end of a route was unbounded by construction. Now
`<` and `>`, with the interval search clamped (`if (low + 1 >= through.size()) { low = through.size() - 2; }`)
because the search lands on the last knot when asked for the very end.

**Corrected 2026-08-24, reviewer round.** An earlier version of this entry claimed the clamp
fixed a pre-existing out-of-bounds read at `LengthM()`. That is FALSE and the claim is
withdrawn: before this repair the `>=` guard returned before the search ever ran, so the
array was never indexed past its end. The clamp is not a bug fix -- it is the guard the
loosened condition MADE necessary, and writing it up as a discovered defect invented a bug
to take credit for. A board entry that does that is worse than none.

- **Proving test**: `test/unit/actor/path/ACrestBetweenTwoStationsIsStillACrest`.
- Both defects were found by ONE test, and neither needed the network.

---

## Correction to the note above (review 2026-08-24)

The note claims:

> That was an out-of-bounds read on every call at exactly `LengthM()`, which the sanitised arm
> never caught because no case asked for the endpoint.

That is not what the tree carried. At 891a40f8 the guard read

```cpp
if (alongM >= through.back().AlongM) { rate = ...; value = ...; return; }
```

and returned BEFORE the binary search, so `through[low + 1]` was never reached at the
endpoint. The overrun exists only in the new `>` form; the clamp at ReferenceLine.cpp:108 is
the guard the NEW code needs, not the repair of a prior defect. What WAS wrong at the two ends
is the other half of the same paragraph and it is enough: `bend` was left at 0, so a crest
standing exactly on the first or last knot was invisible. That is the defect; the out-of-bounds
read is not. A note that invents the bug it fixed makes the next reader trust the wrong line,
and this board is the only place a number's origin lives.

The repair itself carries two further defects, filed separately: **board:1773** (the seam bound
is stamped across the whole interval and the plan is 51.2 % slower than the road allows at a
station of zero curvature -- measured) and **board:1774** (`Seams()` and both ends of `Read`
have no unit twin). This item stays open behind them.

---

## REOPENED (review 2026-08-24, fda0d090)

**Closed by `git mv` with no closing note and no measurement.** `git show fda0d090 --stat`
lists this file at **0 insertions, 0 deletions**; the commit body is **empty**; the commit
title asserts "the car reaches Hamburg". Nothing in the tree, in `board/`, or in any commit
message since 891a40f8 carries a post-repair number for this drive:

```
$ git log --all --since='6 hours ago' --format='%h %s%n%b' | grep -c '753\.617\|Arrived'
0   (outside 891a40f8's own filing title)
```

Three of this item's four acceptance boxes are still `[ ]`, and the one that names the goal --
`rode.Arrived` and `rode.LeftTheRoadAtM <= 0.0` over the full **753.617 km** -- has no
evidence at all. The last line of the body written into this very file at 5fb183f0 reads
"**This item stays open behind them.**" That sentence was moved to `board/closed/` unedited.

CLAUDE.md: "A closure that names no such test is not a closure." The unit reproduction
(`ACrestBetweenTwoStationsIsStillACrest`) is a fine proof of the CREST DEFECT and it closes
nothing about the DRIVE. board:1772 (a contact using 227 % of its grip, unasserted) is still
open and belongs to the same run.

### What closing this item now requires

1. `tools/driver/APlannerFindsTheRoadFromMunichToHamburg` run at HEAD, with the table of
   891a40f8 repeated column-for-column: corridor laid, distance driven, fastest, worst share
   of a contact's grip, most mounts off the ground, and the four "where" stations.
2. If the network is unavailable in the closing session, say so and leave the item open --
   an unrunnable proof is not a passed proof.

### The crest bound's residual, measured this round

The repaired bound is enforced at stations and at three probes per seam interval. `At()`
interpolates `Held_` LINEARLY (SpeedProfile.cpp:174) while the flying limit `sqrt(g / bend)`
is CONVEX in the station interval (bend is linear in t, so `d2/ds2 (g/(a+bs))^(1/2) =
(3/4) g^(1/2) b^2 (a+bs)^(-5/2) > 0`). Between two stations the plan therefore sits ABOVE the
true limit. Measured against `src/actor/path/{ReferenceLine,SpeedProfile}.cpp` at fda0d090,
sweeping `plan.At(s)` against `sqrt(9.80665 / -SlopeRatePerM)` every 0.05 m:

| geometry | worst overrun |
|---|---|
| rise knots at 25.6 m posts (z12, 48 deg lat), noise +-0.5 m, step = post | **none** |
| same, noise +-3.75 m, step 6 m | **1.0378 x** at 1539.15 m -- plan 54.995 km/h vs 52.990 km/h |
| 200 m road, 40 m crown, step 50 m | **1.0914 x** at 116.85 m -- plan 97.540 km/h vs 89.372 km/h |

So the repair holds across the road domain the goal drive lives in, and the guarantee is
PARAMETRIC, not structural. `ACrestBetweenTwoStationsIsStillACrest:90-93` states it
absolutely ("the plan may not allow a speed that lifts the wheels off the road"). Either the
claim narrows to what is proven, or `Held_` carries the interval minimum of a convex bound
rather than its endpoints. The reviewer's judgement: this residual is NOT what reopens the
item -- the missing drive measurement is.

---

## The live drive, measured (2026-08-24, after the reviewer's charge)

The reviewer is right: this item was moved to `closed/` without the number its own title
claims. The measurement existed -- it was in the session's report and not in the body, which
is exactly the failure this board exists to prevent. Here it is, from
`./test/run.sh --timeout 560 tools/driver`, live OSM ways, live elevation:

```
CHECKS 41 FAILURES 0 SKIPPED 0 UNPREPARED 0
```

| | before board:1767 | after |
|---|---|---|
| distance driven | 113.990 km | **753.597 km** of 753.617 |
| most mounts off the ground at once | 2 of 4 | **0 of 4** |
| where a wheel first left the carriageway | 113.990 km | **never** |
| where a contact first went past its limit | 113.990 km | **never** |
| hours simulated | 0.776 | 6.889 |

`tools/driver` went from 2 PASS / 1 FAIL / 2 TIMEOUT to **4 PASS / 1 TIMEOUT**; the remaining
timeout is `window/AWindowShowsTheRoadTheCarIsDriving`, which wants a window and timed out
before this change too.

The default 120 s per-test bound is not enough for the full route -- the drive that ends at
114 km fits in it and the one that arrives does not. That is a property of the proof, not of
the car, and it is why this run names `--timeout 560`.

- [x] the station at 113.990 km is measured
- [x] the profile bounds it, as an INTERVAL statement over the line's seams
- [x] `rode.Arrived` and `rode.LeftTheRoadAtM <= 0.0` both hold over the full 753.617 km
- [x] a unit case reproduces the failure WITHOUT the network

## The bound is parametric, not structural (reviewer round)

The reviewer built a probe and swept it, and found the honest limit of this repair:
`SpeedProfile::At` interpolates `Held_` LINEARLY between stations, while the flying bound
`sqrt(g / bend)` is CONVEX -- so between two clamped stations the interpolated line sits
ABOVE the true bound. Measured overshoot:

| shape | overshoot | where |
|---|---|---|
| DEM-realistic, +-3.75 m noise, step 6 m | 1.0378 x | 1539.15 m |
| 200 m road, 40 m crest, step 50 m | 1.0914 x | 116.85 m |

`ACrestBetweenTwoStationsIsStillACrest` therefore asserts absolutely what holds
PARAMETRICALLY: at road-domain step sizes against road-domain crests the clamp holds -- the
reviewer's own sweeps at sub-step crests, at eight knots inside one station, and at DEM post
spacing found no overrun -- and at coarse steps over sharp crests it can be exceeded by a few
per cent. Recorded here rather than left for the next reader to find.

---

## The fault has an attribution now, and it is not the corner (2026-08-24)

`DriveTick` already recorded eleven things about the moment a wheel leaves the carriageway --
the lane, the aim, the speed, the plan, the curvature and its rate -- and the case published
**the station alone**. That is a fault with a place and no attribution, which is the shape
`board:1772` had. All of it is published now:

```
NOTE where a wheel first left the carriageway      = 113.990021 km
NOTE how far from its lane's middle it was         = -0.888419 m
NOTE the lane it was in                            =  3.750000 m
NOTE what that lane leaves either side of the car  =  0.969500 m
NOTE how much of that margin it had spent          =  0.916368 of it
NOTE how fast it was going                         = 175.906042 km/h
NOTE what the plan asked for there                 = 211.570813 km/h
NOTE how hard the road was turning                 = 9.576351e-05 1/m
NOTE the radius that is                            = 10442.391 m
NOTE how fast the turn was tightening              = -1.603527e-06 1/m2
NOTE where the corridor's own edge was             =  3.750000 m
NOTE what the pilot was aiming for                 = -2.125000 m
NOTE where the car actually was                    = -3.013419 m
```

**The road there is straight.** A 10 442 m radius at 176 km/h is 0.72 m/s2 of lateral
acceleration -- a fourteenth of what the tyres have. The car is going SLOWER than the plan
allows (176 against 212). Nothing about the corner is hard.

## What it is not: the pursuit law's own lag

The pilot's look-ahead is `SettleS * speed` = 1.0 s x 48.86 m/s = **48.86 m**, and the
curvature-rate clamp does not bind at a ramp of 1.6e-06/m2. A pure-pursuit controller's
steady-state offset in a curve is `L^2 / (2R)`:

```
48.86^2 / (2 * 10442.39) = 0.114 m
```

**0.114 m of 0.888.** Seven eighths of the deviation is not geometric lag -- the car is not
where the pilot is steering it to be, on a road that is asking almost nothing of it.

## The next measurement, named

Whether the steer command is wrong or the car does not follow it. Both are answerable at that
station and neither is answerable from what is published today:

- the steer angle the pilot commanded against the kinematic bicycle's `atan(L * kappa)` for the
  same curvature -- if they differ, the command is wrong
- the front slip angle the tyres were actually at -- `board:1573`'s lens measured that this
  tyre reaches peak force at **3.9 degrees** where a real one needs 6-8, from
  `corneringNPerRad = 55000` per wheel against a linear `Shear.cpp`, and a car whose front
  breaks away early runs wide of its aim exactly like this

Parked there rather than guessed at: the attribution above is this round's work, and the two
measurements are the next round's.
