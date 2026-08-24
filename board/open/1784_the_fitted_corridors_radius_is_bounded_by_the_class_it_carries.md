Type: bug
Parent: 1499
Area: generators, actor/path
Tags: drive, geometry, measured

# The fitted corridor's radius is bounded by the class it carries

Measured on the shipped Munich--Hamburg route (`tools/driver/f31.scenario` + Marienplatz to
Rathausmarkt, zoom 10, warm cache, 2026-08-24), by walking `drive.Way.Line` and
`drive.Way.Profile` after `AssembleDrive`:

```
ROUTE 753.617 km, profile step 0.368 m, 2049960 samples
SLOWEST AWAY FROM BOTH ENDS 12.158 km/h at 552.939 km
UNDER 30 km/h: 8710 samples; under 50: 49075; under 80: 226557 (of 2049960)
```

**8 710 samples under 30 km/h is 3.2 km of the route the plan crawls over.** The geometry at the
worst of them, from `ReferenceLine::At`:

| station | kappa (1/m) | radius | plan |
|---|---|---|---|
| 552 920.0 m | 0 | -- | 27.372 km/h |
| 552 925.0 m | +4.139e-02 | 24.2 m | 16.559 km/h |
| 552 930.0 m | +8.212e-02 | 12.2 m | 20.053 km/h |
| 552 940.0 m | **-5.129e-02** | 19.5 m, **sign reversed** | 12.158 km/h |
| 552 945.0 m | **-1.772e-01** | **5.6 m** | 15.085 km/h |
| 552 950.0 m | -1.009e-01 | 9.9 m | 12.158 km/h |
| 552 960.0 m | +6.838e-02 | 14.6 m, **reversed again** | 20.259 km/h |
| 552 970.0 m | 0 | -- | 18.685 km/h |

Slope over the same 50 m is a flat `-0.0105 .. -0.0056`, so this is a PLAN-view defect, not a
crest. **Curvature reverses sign three times in 45 m and reaches a 5.6 m radius** -- tighter
than the F31's own 11.3 m turning circle -- in the middle of a long-distance route.

`SpeedProfile` is not the defect: `sqrt(HoldingMs2 / kappa)` at kappa = 0.177/m is 12.2 km/h,
which is the number printed. **The profile obeys a line that no road has.**

board:1499 states the assumption its own mechanism rests on:

> *a road has a design speed, and the curvature and its rate are bounded by that speed -- which
> is true of a built road and is exactly what OSM's `highway=*` classes imply*

That sentence has no box and no code. `Fit` derives the radius from how far the line may leave
the vertex (`R = within / (shiftShare / cos(theta/2) - 1)`), so a near-doubling-back OSM vertex
pair yields a legal spiral-arc-spiral through a radius no vehicle of that class can take, and
nothing refuses it.

## What will be true

- [ ] The fit takes a MINIMUM RADIUS from the way's class and refuses -- by name, with the
      station and the two vertices -- when the polyline demands tighter, rather than emitting a
      corridor the profile then crawls over.
- [x] Curvature does not reverse sign inside one fitted vertex: a reversal is a junction or a
      refusal, never a fitted artefact.
- [x] A case in `test/unit/actor/path/` builds the two vertices that produce the 5.6 m radius
      WITHOUT the network and requires the named refusal. Negative control: the bound removed
      -> the fit returns the 0.177/m arc and the claim goes red.
- [ ] The drive suite asserts a FLOOR on the plan for the classes it routes over, so 12 km/h in
      the middle of a motorway route is a red verdict rather than a number nobody reads
      (board:1785 is the instrument this needs).

## Comments

- 2026-08-24, reviewer round -- found while testing the main agent's hypothesis that the crest
  clamp (`ClampAround`, `src/actor/path/SpeedProfile.cpp:112-120`) was throttling the drive at
  km 113.990. **That hypothesis is refuted by measurement** (see board:1571): the plan at
  113 990 m is **211.568 km/h**, and the route-wide crest bound is 190.699 km/h at km 479.408
  over 102 binding crests. The crawl is real, it is elsewhere, and it is geometric.

---

## Reproduced without the network, and the reading is sharper (2026-08-24)

The shape is reproducible in code: uneven legs with mixed turn directions.

```
NOTE walked: tightest 6.2873 m at 51.50 m, 3 sign reversals over 125.4 m
```

Three reversals, a radius under seven metres -- the reviewer's route geometry, no OSM needed.

**But the fit is not lying, and that changes what this item is about.** Measured over a sweep
of zigzags at 30 m down to 3 m leg length, and over the wandering shape above:

| | |
|---|---|
| `Fitted::TightestRadiusM` vs a 0.05 m walk of the line | **identical to 1e-9, every time** |
| the fit's own refusal | fires at 7 m legs; at 8 m it lays 6.074 m against a 4.874 m minimum |
| `tightestM` handed to `Fit` | `stood.TightestM`, the vehicle's own centreline minimum (`DriveAssembly.cpp:210`) |

So `Fit` never lays a corner tighter than the minimum it is given, and what it reports is
what the line carries. The 5.6 m arc the reviewer measured is **legal for the F31**, whose
own minimum is 5.65 m -- barely, and that is the point.

**The defect is the BOUND, not the fit.** A 6 m radius is a mini-roundabout or a slip road,
not a route leg: `tightestM` asks "can this vehicle physically turn here", and a corridor
needs "is this a road of the class the route claims". A motorway leg bending to six metres is
an artefact of the graph, and the fit obeys it because nobody told it otherwise. The first
box stands open with that reading, and it is the one that needs a class minimum from the way
data.

Landed:
- `LayCorridor` now refuses a fit whose tightest radius is under the vehicle's own minimum,
  naming both numbers -- a guard against the value ever being handed through slack.
- `Fitted::TightestAtVertex` names the vertex, so the two coordinates that demanded the
  radius can be looked at.
- **Proving test**: `test/unit/actor/path/ACorridorIsFittedThroughVerticesItMayNotLeave` --
  the wandering shape, the walk against the report, and a vehicle needing 7.2873 m being
  REFUSED rather than handed a corner it would crawl.
- **Negative control**: `out.Undrivable > 0` disabled in `Fit` -> `NOTE a vehicle needing
  7.2873 m: still laid`, and an older claim goes red beside it. Reverted.

---

## Reviewer round, 2026-08-24 — the acquittal is right in mechanism and wrong in its number

The mechanism holds. `Fit` cannot lay a corner tighter than the minimum it is handed, and it
is structural, not empirical: `src/actor/path/Fit.cpp:139-143` skips any vertex whose radius
is under `tightestM` and counts it `Undrivable`; `src/actor/path/Fit.cpp:186-194` then refuses
the whole fit. `radiusM[vertex]` is written on no other path, so `TightestRadiusM >= tightestM`
on every laid fit. The sweep did not need to measure that -- it follows from the code. Good.

**The number the acquittal argues from is wrong.** The item says:

> *The 5.6 m arc the reviewer measured is LEGAL for the F31, whose own minimum is 5.65 m --
> barely, and that is the point.*

5.65 m is **half the turning circle**, i.e. the OUTER radius. `Rigging.cpp:170-180` computes
the centreline minimum from it:

```cpp
const double outerM = 0.5 * declared.TurningCircleM;               // 11.3 / 2 = 5.65
out.TightestM = std::sqrt(outerM * outerM - out.Axles.WheelbaseM * out.Axles.WheelbaseM);
```

`tools/driver/f31.scenario:15` declares `turningCircleM="11.3"` and `wheelbaseM="2.810"`:

```
TightestM = sqrt(5.65^2 - 2.810^2) = sqrt(31.9225 - 7.8961) = 4.901673 m
```

So the F31's own minimum is **4.9017 m**, not 5.65 m -- the prose is 15.27 % high and names
the wrong quantity. The measured arc of `1 / 0.1772 = 5.6433 m` clears it by **15.13 %**. It is
not "barely legal"; it is comfortably legal, which makes the item's conclusion *stronger*, not
weaker: nothing about the vehicle was ever going to bound that corner. The item's own sweep
table already carries the right order of magnitude (`a 4.874 m minimum`) and the prose
contradicts it two paragraphs later.

Two consequences:

- The reproduction produced **6.2873 m** where the route measured **5.6433 m**. With the real
  bound at 4.9017 m both are legal, so neither number tests the bound. The reproduction
  reproduces the *shape* (three sign reversals) and not the *case* -- there is still no
  fixture in the tree at which the vehicle bound bites, and the sweep's claim that the fit
  "refuses at 7 m legs" is about a synthetic `kTightestM = 4.874`
  (`test/unit/actor/path/ACorridorIsFittedThroughVerticesItMayNotLeave.cpp:19`), not about the
  F31.
- Every claim message that calls `tightestM` "the tightest radius the vehicle can drive"
  (`src/sim/CorridorLay.cpp:116`, `src/sim/DriveAssembly.cpp:211`) is correct only if the
  reader knows it is the CENTRELINE radius of the rear-axle geometry, not the kerb-to-kerb
  circle the manufacturer prints. That distinction is exactly what this item's first box turns
  on: a class minimum is a road property in the same units, and confusing outer with
  centreline once already produced a 15 % error in this item's own reasoning.

The first box stands open, and the reading in the previous section is upheld: the bound is the
defect. The guard `520f1748` landed against `stood.TightestM` is filed separately as
**board:1791** -- it cannot fail on a corridor with a corner, and it refuses one without.
