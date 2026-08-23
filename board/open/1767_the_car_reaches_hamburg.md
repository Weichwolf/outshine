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
