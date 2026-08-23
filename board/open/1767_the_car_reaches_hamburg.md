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
sample instead, which costs nothing, because slope is continuous across a knot and the
station walk already sees it. Only the second derivative jumps, and only the jump was
invisible.

### A second defect, found by the first

`ReferenceLine::Read` answered a bend of **zero** at both ends of the line:

```cpp
if (alongM <= through.front().AlongM) { rate = ...; return; }   // bend left at 0
if (alongM >= through.back().AlongM)  { rate = ...; return; }   // bend left at 0
```

Those guards are for a station OUTSIDE the knots; `<=` and `>=` made them swallow the first
and last knot too, so a crest at either end of a route was unbounded by construction. Now
`<` and `>`, with the interval search clamped (`if (low + 1 >= through.size()) { low = through.size() - 2; }`)
because the search lands on the last knot when asked for the very end -- and read
`through[low + 1]` past the array. That was an out-of-bounds read on every call at exactly
`LengthM()`, which the sanitised arm never caught because no case asked for the endpoint.

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
