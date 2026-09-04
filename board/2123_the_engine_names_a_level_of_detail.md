Type: debt
State: open
Area: generators, world, render
Tags: architecture, performance
Depends: 2122

# Every drawable carries a level of detail, chosen by ONE rule

**Benchmark** -- Unreal: `LODIndex` per mesh chosen by `ScreenSize`, HLOD clusters baked into
proxy meshes per cell, chosen in the frame. RAGE: LOD models per entity plus a coarser SLOD per
block, baked offline, chosen in the frame. **Both agree** that it is a property in the TYPE
SYSTEM every drawable carries, chosen by projected size, and that the proxy is built when the
geometry is built. Here "offline" is "during preload" -- the substitution board:2110 made for
every generator.

## Where it stands, measured 2026-09-04

```
  the type                 Generators::Detail { Fine, Shell, Massed, Skyline }    include/generate/Generate.h:60
  who obeys it             Forest (lattice spacing), BuildingMesh (rung)
  who ignores it           roads, water, terrain generators
  the ground               a cluster DAG per tile, selected by parent error in the shader -- a
                           SECOND mechanism, board:2122's
  the rule                 buildings: Unseen(errorM, focalPx, awayM) -- a screen-error rule, at INGEST
                           DetailAtRung (Generate.h:79)              -- the rung->level rule, ZERO callers
  GrowsOver                always Detail::Fine (Asking.cpp:115)
  what is baked            coarse OR fine per building, decided at ingest -- not both
```

The concept exists and has two rules, one of them dead. The buildings' geometry per building is
already minimal (14.2 triangles); the COUNT is the whole problem -- 575 805 at Shibuya -- and the
answer is the decision not to mesh most of them at full standing, taken from ONE rule.

## The solution

One rule, the one both references use: projected error in PIXELS. `Unseen(errorM, focalPx,
awayM)` is that rule and it stays; `DetailAtRung` goes, because a rung is a distance in
disguise and the screen is the yardstick. The ground's cluster DAG uses the same quantity --
parent error over distance -- so the two mechanisms become one number read twice.

With board:2122 both levels are BAKED per tile and the frame CHOOSES: a building's fine mesh
and its block's massed shell exist together, and the per-frame choice is a threshold on the
same error. Roads and water carry the rung the same way: a corridor at `Massed` is its
centreline swept at one lane, water at `Massed` is its outline's convex hull.

## What will be true

- [ ] `DetailAtRung` is gone; `Unseen` is the one rule, and the ground's cluster selection
      reads the same error-in-pixels threshold
- [ ] Every generator's `make` honours `Request::Coarseness`: roads, water, terrain too, each
      with a picture at `Massed` looked at
- [ ] Fine and proxy are baked together (board:2122) and the frame chooses; a case moves the
      camera away and the triangle count falls in steps while the picture's horizon does not
      flicker
- [ ] Shibuya draws under the 512 MB ceiling
- [ ] Negative control: force `Fine` everywhere and the ceiling goes RED at Shibuya

## What will show I was wrong

A `Massed` picture whose foreground changed. Measured once: `kMassedAtPx = 8` moved 1.07 % of
OldTown's pixels, all on the horizon, foreground identical -- that is the shape a correct rule
produces, and a rule that moves the foreground is a distance rule wearing a pixel's name.
