Type: feature
State: open
Area: test, generators
Tags: corpora, measured, benchmark

# A synthetic corpus STATES the ground, so the oracle is arithmetic

**Benchmark** — **NEITHER UNREAL NOR RAGE FACES THIS**, and the item says so rather than inventing a
column: Unreal's automation compares a screenshot to a screenshot it took earlier, and RAGE's replay
compares a drive to a drive it recorded -- both grade against THEMSELVES, which is the exact thing a
synthetic corpus exists to avoid. The bodies that DID solve it are cited as evidence: **ASAM
OpenSCENARIO / OpenDRIVE conformance suites** ship synthetic roads whose answer is known by
construction, **`asam-ev/qc-opendrive`** ships 181 PAIRED valid/invalid cases, and **Khronos
glTF-Asset-Generator** generates its own assets and states `loadable: true/false` beside them. All
three agree: **generate the input, never the answer.**

## Why

With real OSM every finding costs forensics — is the defect ours, or a driveway with a 0 m turning
radius? On a terrain declared as `z = f(x, y)` the correct height is KNOWN at every point, so
"a road hovers" becomes a subtraction. Two foreign oracles, neither of them ours:

    the terrain function      mathematics states z(x, y)
    the design standards      RAS-Q / RAA state max gradient, min crossfall, batter

We generate the INPUT. If we also stated the answer it would be agreement with ourselves.

## How: N structures x M terrains

**M = 10 terrains**, each analytic, ascending in what it stresses:

    1  flat                        the null control -- a failure here is unconditional
    2  plane 2 %                   drainage-scale slope
    3  plane 10 %                  at the residential gradient limit
    4  plane 30 %                  beyond every class limit: the road MUST cut or switch back
    5  sine ridge, 200 m / 20 m    the crest vertical curve, and the chord that flew over it
    6  sine valley, 200 m / 20 m   the sag curve, and where water would pool
    7  sine grid, 100 m / 10 m     a 2D field, so no axis is privileged
    8  atanh escarpment, 40 m      a cliff: daylighting or nothing
    9  fBm noise, 5 octaves, 15 m  the realistic case
   10  fBm + escarpment            the adversarial combination

**N = 25 structures**, each declared as OSM-shaped input:

    ways        1 straight · 2 right-angle corner · 3 hairpin at 8 m · 4 doubling back at 0 m
                5 T junction · 6 crossroads · 7 shallow Y fork at 10 deg · 8 roundabout
                9 two ways crossing with NO node · 23 dead end · 24 with width · 25 without width
    vertical   10 bridge over a way · 11 bridge over water · 12 tunnel
               13 embankment (fill) · 14 cutting (cut)
    buildings  15 square · 16 on the fall line · 17 touching a way · 18 sharing a wall
               19 courtyard (a ring with a hole)
    surfaces   20 water polygon · 21 water meeting a way · 22 unsealed track (becomes a CLASS)

**250 cells, not 400.** The number follows from what DIFFERS: a tunnel on flat ground and a tunnel
on a 2 % plane are the same test twice. Cells that are meaningless are declared N/A and that
declaration is itself information -- "a bridge over water on a 30 % plane" says water does not
stand there.

## The oracle, per cell

    the body's underside sits within [z(x,y) - thickness, z(x,y)]     computed
    no two bodies overlap in plan AND height                          computed
    closed, manifold, consistently wound, no self-intersection        computed
    gradient <= the class's maxGradient                               RAS-Q / RAA
    crossfall within [min, e_max]                                     RAS-Q / RAA
    a cut or fill matches the daylight batter                         RAS-Q

## What will be true

- [ ] The terrain is a declared function, not a fetched tile, so a cell runs offline and in
      milliseconds
- [ ] Each cell states its expected answer arithmetically; none of them states OUR output
- [ ] Negative control: a deliberately broken cell -- a road laid 1 m above its own terrain -- goes
      red in every geometric oracle above
- [ ] Real data resumes only when the grid is green, and the grid stays in the gate afterwards
