Type: feature
State: open
Area: world
Tags: instrument, perf

**A car drives the planet, and the world's defects are the finding**

**This engine has no instrument for its WORLD.** `render/` decides subjects against an oracle, `frame/`
decides cost, `scenario/` decides a run -- and nothing decides whether the thing built out of OSM is a
place a body can move through. **A car on a road is that instrument**, and it needs nobody to look at
it.

**A seeded route -- Munich to Hamburg -- driven headless, faster than real time, stopping at the first
place the car cannot hold its line.** **The headless run links NO RENDERER**: the world, the corridors
and the physics, nothing else -- which is what makes it fast and is also the first real test of the
claim that this engine is a library rather than a thing welded to a GPU. The telemetry says WHERE and WHAT KIND, and the same scenario
with a window is the same drive in first person at 60 Hz, so a defect can be looked at.

## Why this is the right instrument and not merely a fun one

**It is falsifiable without a human.** A picture needs an eye; *did the wheels stay on the surface* is a
number per millisecond. **It covers the planet by sampling** rather than by enumeration -- no corpus of
places is needed, only a seed. **And its verdict is a repair**: a jerk at a bridge approach is a
generator defect, a gap is a reader defect, a route through no road is a router defect. *Each class of
finding has a different owner, which is what makes the instrument worth building rather than one bit of
crash/no-crash.*

**TWO FAILURE MODES AND NEITHER SUBSUMES THE OTHER.**

**Having to swerve is already the defect.** The deviation from the ideal line is continuous, per
millisecond, and finds a road that is *bad* long before one that is impossible -- 40 cm of forced
correction is a finding.

**And the crash is still needed, because the worst defects produce NO deviation at all.** A tunnel
nobody recognised gives a road that climbs the mountain instead of going through it: laterally perfect,
continuous, zero deviation, **and a 40 % gradient no vehicle can take.** The car simply cannot get up,
or it leaves the ground at the crest. *A deviation measures a road you can drive badly; a crash
measures a road you cannot drive at all, and the second is where the biggest structural misreadings
live.*

**The headline number is kilometres per finding**, and findings per 100 km by class beside it. That is a
number that starts small, is honest, and drives the work.

## The three-way confound, and it would eat the whole thing

A crash has three candidate causes and they need different repairs:

| | |
|---|---|
| **the data** | OSM genuinely does not say -- a bridge has `bridge=yes, layer=1` and NO HEIGHT ANYWHERE |
| **the generator** | it mis-builds what OSM does say |
| **the instrument** | the vehicle or the autopilot is bad, and a good road looks broken |

**So the negative control comes first and it is not optional**: a synthetic road this engine generates
itself, known smooth by construction, driven at every speed the suite uses. **What jerk does a perfect
road produce?** That number is the instrument floor, and no defect smaller than it may be reported.
`board:1504` carries it and it is that item's first line rather than its last.

## What must be true

- [ ] **A road is a reference line with declared continuity**, so a crack is unspellable rather than
      detectable -- `board:1499`
- [ ] **Every road node gets an elevation and the solve is GLOBAL** -- `board:1500`
- [ ] **The terrain is cut and filled to meet the road**, because a road is not laid on the ground --
      `board:1505`
- [ ] **A bridge looks like a bridge and a tunnel has a portal**, judged by eye -- `board:1506`
- [ ] **A vehicle answers whether it stayed on the road**, per wheel, per millisecond -- `board:1501`
- [ ] **The autopilot is the engine's own** and holds a lane on a road that is correct -- `board:1502`
- [ ] **A route crosses a continent** over a graph that streams -- `board:1503`
- [ ] **The suite runs seeded routes headless and classifies what it finds** -- `board:1504`
- [ ] **A railway is the same corridor with far tighter limits** -- `board:1507` -- and **a train
      cannot steer, which removes a leg of the confound** -- `board:1508`
- [ ] **The same scenario runs with a window, in first person, at 60 Hz**, and stops where the headless
      run stopped -- because *look at every image produced and report what is seen*

## What this feature may NOT do

**It may not make a crash the verdict.** A crash is one bit and the world is a distribution: the suite's
verdict is **kilometres per crash against a declared floor**, with the classes beside it, and a run that
crashed once at kilometre 700 is a better run than one that crashed twice at kilometre 3.

**And it may not repair a defect by making the car better.** An autopilot that steers around a 40 cm
step has hidden the finding. *The vehicle is the instrument and the world is the subject*, so the
vehicle's parameters are declared, pinned, and changed only with a measurement that says the instrument
was wrong.
