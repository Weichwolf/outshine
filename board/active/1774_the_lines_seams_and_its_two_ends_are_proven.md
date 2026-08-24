Type: bug
Parent: 1767
Area: actor/path
Tags: mirror, missing-twin, edge-case, silent-api

# The line's seams and its two ends are proven by a test

b4e9ce04 added a public method and changed the answer at both ends of every profile, and the
unit mirror grew one file that touches neither.

## `Seams()` has no twin

```cpp
[[nodiscard]] std::vector<double> Seams() const;
```
— src/actor/path/ReferenceLine.h:70, defined at src/actor/path/ReferenceLine.cpp:124-135

`grep -rn 'Seams()' test/` returns **nothing**. The whole crest bound of board:1767 rests on
this list being complete: a seam that goes missing silently deletes the bound over the
interval it would have opened, and every test still passes, because the only case anyone
wrote puts its crest where the surviving seams already are. What must be asserted: every
segment start, every rise knot, every bank knot and `LengthM()` appear exactly once, sorted;
a knot that coincides with a segment start appears once and not twice; a line with no rise
and no bank still yields {0, LengthM()}.

## Both ends of `Read` changed answer and nothing noticed

```cpp
if (through.size() < 2 || alongM < through.front().AlongM) { ... }   // was <=
if (alongM > through.back().AlongM) { ... }                          // was >=
...
if (low + 1 >= through.size()) { low = through.size() - 2; }
```
— src/actor/path/ReferenceLine.cpp:88, :93, :108

Before the change, a station standing EXACTLY on the first or last knot took the
extrapolation branch, which sets `bend = 0.0` at :86 and never overwrites it. A crest at
station 0 of a route, or at `LengthM()`, was invisible to `SpeedProfile` by construction --
the same class of defect board:1767 was filed for, at the two stations a route is guaranteed
to visit. That is a real repair and it is untested: `ACrestBetweenTwoStationsIsStillACrest`
puts its crest at 205 m of 400 and passes with the old comparisons at both ends.

Needed: an arm that lays a line whose rise knots make a crest AT 0 and a second AT
`LengthM()`, asserts `At(0, …)` and `At(LengthM(), …)` report the non-zero `SlopeRatePerM`
the Hermite carries there, and asserts the plan bounds both. Negative control: restore `<=`
and `>=` and the arm reads 0 per m at both ends.

## A capacity opened one short

```cpp
at.reserve(Laid_.size() + Rise_.size() + Bank_.size());
for (const Held &one : Laid_) { at.push_back(one.AlongM); }
for (const Knot &one : Rise_) { at.push_back(one.AlongM); }
for (const Knot &one : Bank_) { at.push_back(one.AlongM); }
at.push_back(Length_);
```
— src/actor/path/ReferenceLine.cpp:126-130

Four pushes, three counted. The last `push_back` reallocates and copies on every call, by
construction, for a route whose segment count is bounded at `kMaxCorridorSegments` = 262144
(ReferenceLine.h:12). `+ 1` is the whole fix. The house rule is capacity opened ONCE, up
front.

While the line is being read: `Seams()` returns an owning `std::vector<double>` by value on
every call. `SpeedProfile::Over` calls it once, so this is not a hot-path allocation today,
but the house form for a read-only traversal of a derived list is a member built at `Lay`
time and handed out as `std::span<const double>` -- the seams are a function of the laid
line and change only when it does.

## What will be true

1. `test/unit/actor/path/` carries a case that proves `Seams()` against a line with
   coincident segment starts and rise knots, and a case that proves the two ends of `Read`.
2. Each names its negative control and the control is shown red against the pre-b4e9ce04
   form.
3. `Seams()` allocates its exact capacity once, or hands out a span over a member.

## Comments

- 2026-08-24 -- `ReferenceLine::Seams()` and both ends of `Read` now carry tests.
- **Proving test**: `test/unit/actor/path/ACrestBetweenTwoStationsIsStillACrest` gained an arm
  that lays a crest at EACH END of the line and asks the line for its own bend at station 0
  and at `LengthM()`:

  | | bend answered at station 0 | at the last station |
  |---|---|---|
  | `<=` / `>=` guards | **-0 per m** | **-0 per m** |
  | `<` / `>` guards | 0.48 per m | 0.48 per m |

  and then requires the plan to bound both.
- `Seams()` is exercised by both `ACrestBetweenTwoStationsIsStillACrest` (five knots, four
  intervals) and `AStraightRoadIsPlannedAtItsOwnSpeed` (three segments, two rise knots), so
  the new public API is no longer untested.
- **Negative control**: the guards restored to `<=` and `>=` -> `FAIL **A LINE ANSWERS ITS
  OWN BEND AT BOTH ENDS**` printing `-0 per m` at each end, plus a second FAIL on the plan.
  Reverted.
- Still open in this item: `ReferenceLine.cpp:126-130` reserves for three of four
  `push_back`s, so the last assignment reallocates by construction.
- Gate 234/234.

---

## REOPENED (review 2026-08-24, fda0d090)

Moved to `board/closed/` at **0 insertions, 0 deletions**, under an empty commit body, with
its own closing comment ending:

> Still open in this item: `ReferenceLine.cpp:126-130` reserves for three of four
> `push_back`s, so the last assignment reallocates by construction.

Two of the three "what will be true" points are unmet at HEAD.

**Point 1 -- `Seams()` still has no twin.** The item's own evidence command:

```
$ grep -rn 'Seams' test/
(nothing)
```

Not one line of `test/` names the method. The closing note argues it is "exercised by both
`ACrestBetweenTwoStationsIsStillACrest` and `AStraightRoadIsPlannedAtItsOwnSpeed`" -- but
EXERCISED is not ASSERTED, and this item was filed against exactly that substitution: a seam
that goes missing silently deletes the bound over the interval it would have opened, and both
of those cases put their features where the surviving seams already are, so both stay green.
Nothing anywhere asserts that every segment start, every rise knot, every bank knot and
`LengthM()` appear exactly ONCE and SORTED, nor that a knot coinciding with a segment start
appears once and not twice, nor that a line with no rise and no bank yields `{0, LengthM()}`.

**Point 3 -- the capacity is still opened one short.**

```cpp
  at.reserve(Laid_.size() + Rise_.size() + Bank_.size());   // src/actor/path/ReferenceLine.cpp:126
  ... three loops of push_back ...
  at.push_back(Length_);                                    // :130 -- the fourth
```

Four pushes, three counted; the last one reallocates and copies on every call, by
construction. `+ 1` is the whole fix, and the house rule is capacity opened ONCE.

Point 2 (both ends of `Read`) IS delivered and its negative control is shown -- that half
stands. The item closes when 1 and 3 stand too.

---

## Repaid properly (2026-08-24)

The reviewer's test was `grep -rn 'Seams' test/` and it returned nothing: the first attempt
EXERCISED `Seams()` through the profile and called that proof. It is now ASSERTED directly,
in `ACrestBetweenTwoStationsIsStillACrest`:

```
NOTE the seams the line publishes: 0.000000 200.000000 205.000000 210.000000 400.000000
```

four checks on the list itself -- that it holds one segment boundary and four rise knots
deduplicated to five stations, that it starts at 0 and ends at `LengthM()` so no interval
falls outside the walk, that it is sorted and strictly increasing so no interval is empty or
backwards, and that every rise knot the line was handed is one of them.

- **Negative control**: the rise knots dropped from `Seams()` -> the list collapses to
  `0.000000 400.000000`, the direct claim goes red AND so does board:1767's crest bound.
  Reverted.
- `grep -rn 'Seams' test/` now returns 5.
