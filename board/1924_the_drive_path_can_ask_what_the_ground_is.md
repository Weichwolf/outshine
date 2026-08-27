Type: task
State: open
Parent: 1919
Area: engine, world
Tags: reachability, measured

# The drive path can ask what the ground at a point IS, not only how high it is

**Benchmark** — Unreal: a landscape query returns height AND the layer weights at a point, so what the ground IS comes back with how high it is. RAGE: material per polygon under the wheel. **Both agree** — the query answers the surface, not only the altitude.

The capability exists and the drive cannot reach it. `ClassStructure::Evaluate(e, n, *distM,
*runnerUp)` returns the surface class at an east/north point, and `src/engine/Sim.cpp:547`
already calls it for the picture. `Generators::Infrastructure::MadeAt` returns a `Made` carrying
`CoverRow` and `SurfaceAslM` -- the made surface's material AND its height.

The drive path stands up neither. `Engine::State` holds `Ground::GroundStack`
(`src/engine/Engine.cpp:117`), which owns a `TilePool` and a `GroundStream`: height at a lat/lon
and nothing else. The class field and the infrastructure generator live in `Ground::World`, which
the drive never opens.

So the drive knows how HIGH the ground is and not WHAT it is, and board:1919's remaining half --
a wheel's height and normal from the surface under it -- has nowhere to ask.

This is the reachability class again: `DrawsSky`, `ShadowRadiusM`, the shadow atlas centre. A
capability complete, correct and unreachable.

## What stands now

`Sim::Underfoot` is the seam: `At(lat, lon) -> Standing{Known, HeightAslM, Friction}` plus
`PostM(lat)`. `Sim::GroundUnderfoot` fills it from the stack's height stream and its class field,
resolving the class through `VegetationTemplates::FrictionOf`. `GroundStack` now carries the
`ClassField` and opens it at the focus, so the drive path can ask what the ground IS and not only
how high it is. The projection-plus-evaluation is spelled ONCE, in `ClassField::ClassAt`, and
`Sim::At` was rewritten onto it.

`DriveTick` asks it only for a wheel PAST the made surface -- inside the corridor the ribbon is
the surface and a query per wheel per step would buy nothing. The asks and the answers are
counted and published, because a class field that never resolves would otherwise fall back
silently. The normal off the made surface is a finite difference at the terrain's own post
spacing, which is board:1937's first line: the field holds normals and the sample drops them.

## What is still wrong

The made surface's own height still comes from the ribbon rather than from
`Infrastructure::MadeAt`, which carries `SurfaceAslM` and `CoverRow`. That is right today because
the ribbon IS what the corridor made -- but it will not be right once a drive crosses a made
surface it did not lay itself.

## What will be true

- [x] A drive can ask, for a world point, what a wheel would stand on: its height and what it
      grips with.
- [ ] The query is bounded: two lookups and no allocation, and only for a wheel that is off the
      made surface. **This tick was wrong when it was written.** The off-made query costs THREE
      `At()` calls, not two -- the extra pair build a finite-difference normal -- and each one
      reaches `TilePool::FetchInto`, which sleeps up to 30 s, allocates, and takes a mutex on the
      way. Measured and cited in board:1937, which owns the repair.
- [x] It is ONE query. `ClassField::ClassAt` is the single spelling and `Sim::At` uses it.
- [ ] It answers for MADE ground a drive did not lay: `Infrastructure::MadeAt` reaches the
      contact, so a bridge deck or a car park is a surface like any other.
