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
