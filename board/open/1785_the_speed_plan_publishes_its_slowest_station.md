Type: bug
Parent: 1522
Area: actor/path
Tags: instrument, telemetry, drive

# The speed plan publishes its slowest station

`SpeedProfile` publishes three numbers about the crest term and NOTHING about the plan as a
whole (`src/actor/path/SpeedProfile.h:53-56`):

```
[[nodiscard]] double CrestHeldMs() const { return CrestHeld_; }
[[nodiscard]] double CrestHeldAtM() const { return CrestHeldAt_; }
[[nodiscard]] size_t CrestsThatBound() const { return CrestsBound_; }
```

The crest term got its instrument because board:1767 needed it. The other four terms --
cornering, slip, curvature rate, climb -- bound the plan silently, and the plan's own worst
value is nowhere. So:

- a drive can be planned at **12.158 km/h at km 552.939** of a 753.617 km motorway route and
  every check in `tools/driver/APlannerFindsTheRoadFromMunichToHamburg` still passes, because
  the case asserts the FASTEST it went (`rode.TopMs`) and never the slowest the plan allowed;
- **8 710 of 2 049 960 stations (3.2 km of route) plan under 30 km/h** and no number says so;
- attributing a crawl needs a bespoke tool -- which is what this round had to write to answer
  the question at all.

Every number above was obtained by walking `SampleAt()` from outside. That is the whole point:
the data is there, the STATEMENT is not, so nothing can regress on it.

## What will be true

- [ ] `SpeedProfile` publishes `SlowestMs()`, `SlowestAtM()` and, per bounding term, the count
      of stations where that term was the binding one -- the same shape `CrestsThatBound()`
      already has, generalised over the terms instead of privileging one.
- [ ] A station's binding TERM is nameable: "cornering", "slip", "curvature rate", "climb",
      "crest", "top speed" -- so a crawl says which physics produced it without a debugger.
- [ ] The drive cases assert a floor and its population: p50/p95/p99 of the plan over the
      route, and the count of stations under a declared floor, so board:1784's 3.2 km of
      sub-30 km/h plan is a red verdict.
- [ ] Negative control: the 5.6 m radius of board:1784 planted in a unit corridor -> the
      slowest-station number moves and the floor claim goes red.

## Comments

- 2026-08-24, reviewer round -- filed beside board:1784, which is the geometry this instrument
  would have caught. Order matters: the instrument is cheap and the geometry is not, so the
  instrument goes first and the repair is then measurable.

## Comments

- 2026-08-24 -- repaid. `SpeedProfile` publishes which of its five terms holds each station,
  and which station is slowest:

```cpp
enum class Held : uint8_t { Free, Curvature, Slip, Ramp, Climb, Crest, kCount };
struct Standing { double Ms; double AtM; Held By; };
[[nodiscard]] Standing Slowest() const;
[[nodiscard]] size_t BoundBy(Held term) const;
[[nodiscard]] static const char *NameOf(Held term);
```

  `NameOf` carries board:1787's `static_assert` on its own table, so a sixth term cannot be
  added without a name.
- **Measured**, straight 1000 m + spiral 500 m + arc 300 m:

```
the slowest station = 109.882 km/h at 1500.0 m, held by 'curvature'
stations held by 'free' = 52, 'curvature' = 39, 'slip' = 0, 'ramp' = 0, 'climb' = 0, 'crest' = 0
```

  Every station is accounted to exactly one term, and the tally sums to `SampleCount()` --
  a tally that does not add up to the plan is a tally of something else.
- **Proving test**: `test/unit/actor/path/AStraightRoadIsPlannedAtItsOwnSpeed`, five checks.
- **Negative control**: the curvature term stopped naming itself -> the slowest station
  reports `'free'` at 1800.0 m and two claims go red. Reverted.
- This is the instrument the reviewer had to build by hand to find board:1784's 5.6 m radius.
  The plan names it now: a station bound by `curvature` at a speed no vehicle would choose is
  a corridor defect, and it says so without a probe.

---

**REOPENED by the hourly review, 2026-08-24 (`e0f87385` disproved on its own fixture).**

`Slowest()` and `BoundBy()` are computed inside the sampling loop at
`src/actor/path/SpeedProfile.cpp:134-141` — and `Over()` then lowers `Held_[]` **three more
times** before it returns:

| pass | file:line | what it lowers |
|---|---|---|
| seam clamp `ClampAround` | `src/actor/path/SpeedProfile.cpp:146-178` | stations either side of every seam |
| entry + acceleration sweep | `src/actor/path/SpeedProfile.cpp:183-188` | `Held_[0] = min(entryMs, …)`, then forward `sqrt(v² + 2aΔ)` |
| braking sweep, backwards | `src/actor/path/SpeedProfile.cpp:189-192` | every station on the approach to a bound one |

Nothing recomputes `Slowest_` or `Bound_` after any of them. The published telemetry
describes an **intermediate array that no consumer can observe**, and it is presented as a
statement about the plan.

## The measurement, on the fixture the closing commit used

Same road (straight 1000 m + spiral 500 m + arc 300 m), same F31 envelope, same
`entryMs = 0.0` that `test/unit/actor/path/AStraightRoadIsPlannedAtItsOwnSpeed.cpp:57`
passes, 91 stations at 20 m:

```
PUBLISHED slowest = 30.5226 m/s (109.882 km/h) at 1500.0 m, held by 'curvature'
ACTUAL    slowest =  0.0000 m/s (  0.000 km/h) at    0.0 m (sample 0 of 91)
VERDICT   THE PLAN DOES NOT KNOW ITS OWN SLOWEST STATION
```

`plan.SampleAt(0)` is **0.0 m/s** — the car starts from rest, which is what `entryMs = 0.0`
declares — while `Slowest()` answers 109.882 km/h at 1500 m. The commit message reports the
second number as a measurement of the plan. It is a measurement of something the plan threw
away.

The tally is worse than stale, it is false:

```
topMs = 150.303 m/s
stations whose PLAN speed is below topMs: 91 of 91
stations the plan tallies as 'free':      52
```

**Zero of 91 stations are free. The plan says 52.** Station 0 is 0.000 m/s and is tallied
`free`; stations 1..5 are 22.3, 31.5, 38.6, 44.6, 49.8 m/s — the traction-limited launch —
and every one of them is tallied `free`. The whole kilometre of straight is brake-limited on
its approach to the bend; that is exactly what this file's own `board:1773` claim asserts at
`test/unit/actor/path/AStraightRoadIsPlannedAtItsOwnSpeed.cpp:95-104`, and the tally
contradicts it.

`CHECK(counted == plan.SampleCount())` cannot catch any of this: `Bound_` is incremented once
per loop iteration, so the tally sums to `SampleCount()` by construction, whatever it counts.
It is a proof that a counter was incremented, not that a station was accounted.

## Why the test went green

`test/unit/actor/path/AStraightRoadIsPlannedAtItsOwnSpeed.cpp:120-121` checks
`CHECK_NEAR(slowest.Ms, inTheBendMs, 1e-9)` — against the **analytically recomputed bend
limit**, i.e. against the same intermediate the code computed. The test's oracle and the
defect share a source. Not one assertion in the block compares `Slowest()` to
`min_i plan.SampleAt(i)`, which is the only statement `Slowest()` can honestly make.

## What must be true

1. `Slowest()` answers `min_i SampleAt(i)` and the station it stands at — or it is renamed to
   what it actually holds (`TightestLimit()`) and the plan gains a separate, correct
   `Slowest()`. A getter named `Slowest` that is not the slowest is worse than no getter.
2. `BoundBy(term)` accounts stations **as the returned plan stands**, which means the
   catalogue must grow the terms that actually bind it: `Entry`, `Traction`/`Accel`, `Brake`,
   `Seam`. Six terms that cannot name what holds 52 of 91 stations are not a catalogue.
3. The proving test compares the published numbers to the plan's own array, never to a
   recomputed analytic limit: `CHECK(slowest.Ms == min_i SampleAt(i))` and
   `CHECK(BoundBy(Free) == count of stations at topMs)`.
4. Negative control the closure must show: the seam/accel/brake sweeps disabled → the two
   views coincide; enabled → the test is red against today's code. Today's code passes the
   test that was supposed to prove it.

## Hygiene found in the same file

- `SpeedProfile.h:57` — `BoundBy(Held term)` indexes `Bound_[(size_t)term]` with no guard;
  `BoundBy(Held::kCount)` is a read one past the array, and `kCount` is a spellable
  enumerator of a public enum. Either the sentinel leaves the public type or the query
  refuses.
- `SpeedProfile.h:55-64` — every one of these queries is a trivial member read and not one is
  `noexcept`, three of them not `constexpr`.
- `SpeedProfile.cpp:72` — `HeldAt(const Placed &, Held *by = nullptr)` is a defaulted
  out-parameter pointer in a C++23 tree; the value form (`struct { double Ms; Held By; }`)
  says the same thing without a nullable seam, and the three call sites at
  `SpeedProfile.cpp:169-171` that take the default are exactly the ones whose term is thrown
  away and then reported as `free`.
- `SpeedProfile.cpp:9-13` — the name table's `static_assert` message spells `board:1787`.
  See `board:1654`, reopened this round.

---

## Reopened correctly, and the closure was wrong (2026-08-24)

The reviewer measured what I published against what the plan holds, and the two are different
objects. `Over()` lowers `Held_[]` **three more times** after the sampling loop -- the seam
clamp, the acceleration sweep, the backward brake sweep -- and the telemetry was computed
before any of them.

| | |
|---|---|
| what I published | 30.5226 m/s (109.882 km/h) at 1500.0 m, held by `curvature` |
| what the plan holds | **0.0000 m/s at 0.0 m**, held by `entry` |
| stations below `topMs` | **91 of 91** |
| stations my tally called free | **52** |

**And my own proving test could not catch it**, which is the worse half: it asserted
`slowest.Ms` against the analytically recomputed cornering limit -- the same source the
defect came from. Oracle and defect agreed, so the claim was green about a plan that was
never used. `CHECK(counted == plan.SampleCount())` could never catch it either: `Bound_` was
incremented once per loop pass, so the sum was right by construction while every entry was
wrong.

The repair:

- `Why_[]` records, per station, which term last lowered it -- through the sampling loop, the
  seam clamp, and both sweeps.
- `Slowest_` and `Bound_` are computed AFTER the last sweep, from `Held_` and `Why_`.
- Four terms the plan had no name for are named: `Seam`, `Entry`, `Traction`, `Brake`. The
  reviewer named all four as missing.
- `BoundBy(Held::kCount)` no longer reads past the array -- `kCount` is publicly spellable.

The test now uses the PLAN as its own oracle:

```cpp
CHECK_NEAR(slowest.Ms, leastMs, 1.0e-12, "m/s", ...)   // leastMs = min over SampleAt(i)
CHECK(plan.SampleCount() - plan.BoundBy(Held::Free) == belowTop, ...)
```

- **Negative control**: the telemetry moved back into the sampling loop -> `109.882 km/h at
  1500.0 m` and `52` free stations, both claims red. Those are exactly the numbers the first
  closure offered as proof.
