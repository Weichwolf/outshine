Type: chore
State: open
Area: generators, world
Tags: measured, determinism

# The generators draw from ONE unit random, seeded by the PLACE

**Benchmark** -- RAGE seeds one generator per system and replays a drive from it, which is only
possible because there is one; Unreal's `FRandomStream` is a single named type every system
holds an instance of. **Both agree: one implementation, many streams.** And a generator is the
most dangerous place in the tree for determinism, because it is the only part that rolls dice:
a seed taken from a counter depends on the order tiles arrive in.

## Where it stands, measured 2026-09-04

| where | algorithm | to unit interval |
|---|---|---|
| `generators/building/BuildingShape.cpp:121` | the 32-bit bit mixer (`0x7feb352d`, `0x846ca68b`), file-local `UnitOf` | `>> 8` / 2^24 |
| `generators/Structures.cpp:24` | splitmix64 | `& 0xFFFFFF` / 2^24 |
| `world/ground/AlpineLimit.cpp:12` | three custom words | `& 0xFFFFFF` / 2^24 |
| `generators/flora/TreeRandom.h` | xorshift32, used by every tree stage | `>> 8` / 2^24 |

Four intentions written four times, all ending in 24 bits over 2^24. `Tile::Seed`
(`generators/base/Tile.cpp:23`) is already a proper splitmix64 seed mixer in the shared base and
is the one to build on.

## The solution

One `UnitOf(seed, stream)` in `generators/base/`, over `Tile::Seed`'s mixer, and every draw
passes a stream id. The seed is `PlaceHash(LongitudeLatitude)` -- what storey counts already use
-- so a tree at a place branches the same way whatever order its tile arrived in. The xorshift
STATE in `TreeRandom` stays as a stream that `UnitOf` seeds, because a grower needs a sequence
and not a hash; what goes is its own seeding.

## What will be true

- [ ] One mixer, one `UnitOf`; the four above call it with a stream id
- [ ] Every generator seed is a function of the PLACE, never of a counter; a case shuffles tile
      arrival order and the picture does not move
- [ ] The commit renders all nine places and says their digests moved and why -- every
      building's proportions, every tree's branching and every treeline's jitter change once

## What will show I was wrong

If `make shots` moves a digest AFTER the unification on a second run, the seed still reaches a
counter somewhere and the case above has to find it before this closes.
