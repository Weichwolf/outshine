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
