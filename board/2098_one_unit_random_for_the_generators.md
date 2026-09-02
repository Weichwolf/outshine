Type: chore
State: open
Area: generators, world
Tags: measured

# The generators draw from ONE unit random, and its sequence is declared

**Benchmark** — RAGE seeds one generator per system and replays a drive from it, which is only
possible because there is one; Unreal's `FRandomStream` is a single named type every system holds an
instance of. Both answer the same way: **one implementation, many streams**. This tree has three
implementations.

## Measured 2026-09-02, while naming their constants

| where | algorithm | to unit interval |
|---|---|---|
| `src/generators/draw/BuildingShape.cpp` | the published 32-bit bit mixer (`0x7feb352d`, `0x846ca68b`) | `>> 8` then `/ 2^24` |
| `src/generators/Structures.cpp` | splitmix64 (`6364136223846793005`, `0xff51afd7ed558ccd`) | `& 0xFFFFFF` then `/ 2^24` |
| `src/world/ground/AlpineLimit.cpp` | three custom words (`0x8da6b343`, `0x2c1b3c6d`, `0x297a2d39`) | `& 0xFFFFFF` then `/ 2^24` |

All three end the same way -- 24 bits over 2^24 -- and all three get there differently. That last
column is the tell: they are the same INTENTION written three times.

## Why board:2093 named them instead of merging them

A hash is not a spelling. Unifying these changes the sequence every one of them draws, so every
generated building's proportions, every tree's branching and every treeline's jitter moves. That is
a change to what the engine DRAWS, and it belongs in a commit whose digests are expected to move and
which says which picture is the new one -- not in a pass that was making numbers readable.

## Done when

One `UnitOf(seed, stream)` in the generators' own door, one mixer behind it, and the three call sites
pass a stream id instead of carrying an algorithm. The commit that does it renders all nine places
and states that their digests moved and why.
