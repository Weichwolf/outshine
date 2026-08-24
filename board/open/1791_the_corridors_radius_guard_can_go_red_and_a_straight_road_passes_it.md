Type: bug
Parent: 1784
Area: sim, actor/path
Tags: drive, geometry, measured, refusal

# The corridor's radius guard can go red, and a straight road passes it

`520f1748` landed a new refusal in `LayCorridor`:

```cpp
say.Claim(fitted.TightestRadiusM >= tightestM,
      "**AND NO CORNER IS TIGHTER THAN THE CAR CAN DRIVE.** ...");
if (fitted.TightestRadiusM < tightestM) { return false; }
```
`src/sim/CorridorLay.cpp:117-124`

It is defective in **both** directions.

## 1. It refuses a corridor that has no corner

`Fitted::TightestRadiusM` is only ever written inside the interior-vertex loop
(`src/actor/path/Fit.cpp:153-156`). A fit with no bend never enters it, so the field keeps its
default `0.0` -- and `0.0 >= 5.65` is false. Measured against the tree at `520f1748`, F31
minimum:

```
two-point straight  : Laid=1  TightestRadiusM=0.000000  len=1000.000  -> guard REFUSES
collinear three-pt  : Laid=1  TightestRadiusM=0.000000                -> guard REFUSES
one bend            : Laid=1  TightestRadiusM=17.053393               -> guard passes
```

A straight route -- two coordinates on one way, or a polyline `Simplify`
(`src/actor/path/Fit.cpp:59-74`) collapses to collinear vertices -- now makes `LayCorridor`
return `false` with the claim **AND NO CORNER IS TIGHTER THAN THE CAR CAN DRIVE** red. There
is no corner at all. A refusal must name what is wrong; this one names the opposite.

## 2. On any corridor that HAS a corner it cannot fail

`Fit` already refuses tighter than the minimum it is handed, one screen above:

```cpp
const double radius = CornerRadiusM(turn, shorter, withinM);
if (radius < tightestM) { ++out.Undrivable; ...; continue; }   // Fit.cpp:139-143
...
if (out.Undrivable > 0) { out.Error = ...; return out; }        // Fit.cpp:186-194
```

`radiusM[vertex]` is written only on the `radius >= tightestM` path, `TightestRadiusM` is the
minimum over exactly those, and `Undrivable > 0` makes `Laid` false. `LayCorridor` returns at
`src/sim/CorridorLay.cpp:116` when `!fitted.Laid`. So by the time the new claim is evaluated,
`TightestRadiusM >= tightestM` is **true by construction** whenever it is non-zero. The guard
proves that `Fit` did what `Fit` cannot avoid doing.

## 3. And the number it prints is wrong on a refused fit

`TightestRadiusM` is populated even when the fit refuses -- it is the minimum over the
*drivable* vertices, i.e. it excludes precisely the vertices that caused the refusal:

```
wandering polyline, tightestM = 5.65:
  Laid=0  Undrivable=2  TightestRadiusM=15.440427  TightestAtVertex=1
```

`15.44 m` is published as "the tightest radius the fit produced" for a fit that refused two
vertices for being too tight. `Fitted::TightestAtVertex` (added by the same commit) has the
same hole: `0` means both "vertex 0" and "no vertex was ever tightest".

## What will be true

- [ ] `Fitted` distinguishes "no corner" from "a corner of radius R": either
      `std::optional<double>`/`std::expected`, or a companion `Corners` count the guard
      consults before comparing. A straight corridor is laid, not refused.
- [ ] Either the guard is removed as vacuous, or it is moved to where it can fail -- the
      class minimum board:1784's first box asks for, which `Fit` is NOT handed today.
- [ ] `TightestRadiusM` and `TightestAtVertex` are not published for a refused fit, or they
      report the tightest DEMANDED radius including the undrivable vertices, which is the
      number a reader of that refusal wants.
- [ ] `Fitted::TightestAtVertex` is read by something, or it is deleted. Nothing in `src/`
      reads it today (`grep -rn TightestAtVertex src/` -> one write, no read).
- [ ] Negative control: a two-vertex straight route through `LayCorridor` -> red against
      today's code, green after.

## Comments

- 2026-08-24, reviewer round -- measured with a standalone probe against
  `src/actor/path/Fit.cpp` and `src/actor/path/ReferenceLine.cpp` at `520f1748`, no network.
  `src/sim/CorridorLay.cpp` has no unit twin (see board:1624, reopened this round), so no
  test in the tree evaluates this guard at all: `unit/actor/path` is 14/14 PASS with the
  defect standing.

## Comments

- 2026-08-24 -- the guard is GONE, which is the honest repair. It could not work in either
  direction, and the reviewer proved both with a standalone probe I reproduced:

```
two-point straight     laid=1 TightestRadiusM=0.000000 -> guard REFUSES
collinear three-point  laid=1 TightestRadiusM=0.000000 -> guard REFUSES
one bend               laid=1 TightestRadiusM=823.142342 -> guard passes
```

  `TightestRadiusM` is written only inside the interior-vertex loop
  (`src/actor/path/Fit.cpp:153-156`), so a route with no corner never enters it and the field
  stays 0.0. **My guard refused a straight road for having a corner tighter than the car can
  drive.** And on any route WITH a corner it was tautological: `Fit.cpp:139-143` skips every
  vertex under `tightestM` and `:186-194` refuses the whole fit, so the property is
  structural and needs no caller to re-check it.
- A check that cannot hold and produces false red is worse than no check. Removed rather than
  patched, because patching it would have kept a caller-side restatement of a guarantee the
  producer already makes.
- **Proving test**: `ACorridorIsFittedThroughVerticesItMayNotLeave` gained the three shapes
  above and asserts the property as it actually is: `TightestRadiusM == 0.0` when there is no
  corner, `>= tightestM` when there is. A caller reading it as a plain minimum refuses
  straight roads, and the test says so.
- **Negative control**: the assertion written as a plain `>= kTightestM` -> the two-point and
  collinear shapes go red, which is exactly the defect this item names.
- Not repaid: `TightestRadiusM` still overloads 0.0 as "no corner" and the reviewer's related
  reading -- that a REFUSED fit publishes the minimum over the DRIVABLE vertices, excluding
  the ones that caused the refusal -- stands open here.
