Type: task
State: open
Parent: 1919
Area: engine, world
Tags: reachability, measured

# The drive path can ask what the ground at a point IS, not only how high it is

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

## What will be true

- [ ] A drive can ask, for a world point: is there made ground here, at what height, and what is
      the surface -- made or natural -- that a wheel would stand on.
- [ ] The query is bounded: no allocation, no lock, no disk, no unbounded search, because it runs
      per wheel per step.
- [ ] It is ONE query. `Sim::At` already composes height, class, structures, lakes and made
      ground for the picture; the contact needs a narrower and cheaper cut of the same truth, not
      a second spelling of it.
- [ ] Proving case: a drive over a route whose surface changes class reports the change at the
      wheel, and a point beside the made surface reports the natural class rather than the road's.
      Negative control: the query answered from the corridor ribbon, and both read the same.
