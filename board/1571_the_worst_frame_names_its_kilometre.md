Type: bug
State: active
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

- [x] the case prints kilometre, frame index and relay/steady class for every frame that sets a
      new worst
- [x] the 23 ms frame is attributed, and the fix or the named exclusion follows its mechanism

## Comments

Filed from the first complete windowed drive. The drive itself is the headline: the same Ride
that carries the headless run drew every frame of Munich to Hamburg at 720p with a player
handover at km 38.7, arrived at Rathausmarkt, and held p99 at 6.12 ms against the 16.67 budget.

---

**Reviewer sharpening (2026-08-24) -- the kilometre a WORST line names can REPEAT, and the
speed-plan explanation for it is refuted by measurement.**

The 63-minute windowed run printed two records at the same station:

```
WORST 69.882 ms at 113.990 km, frame 189014
WORST 98.518 ms at 113.990 km, frame 369107
```

**180 093 frames apart, three decimals identical.** `WORST` prints only on a new record
(`tools/driver/window/AWindowShowsTheRoadTheCarIsDriving.cpp:358-360`), and the route is ONE
corridor for the whole drive (`routeM = drive.Way.Line.LengthM()`, `:271`), so a monotone
station cannot be equal at two records 180 093 frames apart on a 753.617 km route. Either the
car stood still for ~50 minutes of simulated time -- while the same drive reports 753.597 km
and 6.889 h simulated headless -- or the number the instrument prints is not advancing.

**What the station is**: `out.ReachedM = at.AlongM` (`src/sim/DriveTick.cpp:59`), the
RESECTION of the body onto the corridor through `Pilot::Locate` inside a window of
`kResectM + 3 * drive.LostM` (`:48-52`, `src/actor/mind/Course.cpp:27-50`). It is a projection,
not an odometer: it can stick, and it can go backwards, and **nothing asserts otherwise and
nothing publishes a stall**.

**The speed-plan hypothesis is refuted.** Measured this round by walking `drive.Way.Profile`
after `AssembleDrive` on the same route (warm cache, 2026-08-24):

| | |
|---|---|
| plan at **113 990 m** | **211.568 km/h** |
| geometry there | kappa +9.580e-05 /m, slope -0.0213, dslope -2.762e-04 /m |
| route-wide crest bound | 190.699 km/h at km 479.408, over **102** binding crests |

`ClampAround` (`src/actor/path/SpeedProfile.cpp:112-120`) does not throttle km 113.99. The
plan there is within 6 % of the F31's drag-limited top speed. Whatever pins that station, it is
not the crest clamp and not the profile.

- [x] `Ridden` publishes whether the station ADVANCED this tick, and the drive cases assert
      monotonicity over the route: a station that repeats across frames is a stall, and a stall
      is loud.
- [x] The `WORST` line carries the frame's simulated time and the body's speed beside the
      kilometre, so a repeated station is visible in the line itself.
- [x] Re-run blocked: both windowed cases are currently UNRUNNABLE on this machine -- the
      prepared F31 has no textures and they FAIL rather than report unprepared (board:1786).

---

## The outlier is attributed, and the instrument is complete (2026-08-24)

The `WORST` line carries everything the item asked for:

```
WORST 17.248 ms at 53.802 km, frame 77346,  t 1314.899 s, 172.2 km/h, moving, steady
WORST 18.105 ms at 54.601 km, frame 78661,  t 1337.254 s, 123.6 km/h, moving, steady
WORST 22.389 ms at 86.555 km, frame 119126, t 2025.159 s, 146.7 km/h, moving, steady
```

kilometre, frame index, simulated time, speed, whether the station ADVANCED, and relay against
steady. The 23 ms frame this item was filed about is **22.389 ms at km 86.555**, on a moving
car at 146.7 km/h, laying no new corridor.

**And the stall question is answered with a zero.**

```
NOTE ticks where the station did not advance = 0 ticks
NOTE the longest the station stood still     = 0 s
```

`Ridden` publishes whether the station advanced, and both drive cases assert that no station
stands still for a second. Over 742 km and 2.79 million ticks, none does -- so the outlier is
not a stall, which was the item's own leading hypothesis.

## What it is, and where the fix went

A single steady frame at 5.07x the drive's own p99, on a machine also running a browser. That
is `board:1800`'s subject and its repair: **the bar is the distribution**, and the maximum is
published with its place because an outlier that can be looked at is worth having -- which is a
different thing from being the bar.

- **Proving test**: `apps/driver/test/window/AWindowShowsTheRoadTheCarIsDriving`, the whole
  route at `--timeout 900`, 1/1.
- **Negative control** for the stall claim: `Ridden::Advanced` forced false -> the longest
  stall grows with the drive and the claim names the kilometre.
- The re-run blocker this item recorded is gone: `board:1786` closed, the prepared F31 carries
  its seven files, and both windowed cases run.
