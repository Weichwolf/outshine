Type: bug
Area: render
Tags: perf, instrument

**The worst frame names its kilometre**

The full windowed drive (2026-08-22): 1 469 414 frames over 774.851 km, p50 1.87 / p95 4.53 /
p99 6.12 ms -- and ONE steady frame of 22.99 ms, present as the running worst from before km 400
and never exceeded, failing the frame-floor check while every distribution check passes with 2.7x
margin.

Two defects, one item:

- **the instrument**: the case records the worst frame's cost and NOT its kilometre, its frame
  index or what the frame did -- so a 23 ms outlier in 1.5 million cannot be attributed. A cost
  that cannot be attributed is not a finding
- **the frame**: once attributed, either it is the first frame's warm-up (pipelines compiling,
  first uploads -- then the case starts its clock after the first presented frame, named), or it
  is a real hitch with a mechanism

- [ ] the case prints kilometre, frame index and relay/steady class for every frame that sets a
      new worst
- [ ] the 23 ms frame is attributed, and the fix or the named exclusion follows its mechanism

## Comments

Filed from the first complete windowed drive. The drive itself is the headline: the same Ride
that carries the headless run drew every frame of Munich to Hamburg at 720p with a player
handover at km 38.7, arrived at Rathausmarkt, and held p99 at 6.12 ms against the 16.67 budget.
