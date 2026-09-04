# The engine names a level of detail

State: open

```
grep -rIn '\bLod\b|\bLOD\b' src/ include/   ->   0
```

**A game engine with no level of detail in its vocabulary.** Measured 2026-09-04, and it explains
the memory and the frame times at once rather than as two problems.

## What is actually there, and why it is not LOD

- **the ground has it**: tiles carry zoom levels, and a cascade of rungs
- **the buildings have a hint of it**: `BuildingField::Lump` merges neighbours into a block, which
  is RAGE's SLOD idea by another name
- **nothing else has any**, and the buildings' version barely fires:

```
  Shibuya   total=252338   lumped=3832  blocks=3612    1.5%
            total=280057   lumped=473   blocks=443     0.2%
  awayKm=5.62 -- every building within 5.6 km is meshed individually
```

## The measurement that killed the obvious explanation

I assumed 618 MB of building geometry meant buildings were too detailed. They are not:

```
  575805 buildings, 8154327 triangles  ->  14.2 triangles a building
```

A cuboid is 12. **The geometry per building is already minimal; the COUNT is the whole problem.**
575805 buildings meshed at once, against OldTown's 37057. No amount of simplifying one building
helps -- what is missing is the decision not to mesh most of them at full standing.

## What the references do, and they agree

| | Unreal | RAGE |
|---|---|---|
| per mesh | `LODIndex` chosen by `ScreenSize` | LOD models per entity |
| per area | HLOD clusters baked into proxy meshes | SLOD per block |
| when | baked offline, chosen in the frame | baked offline, chosen in the frame |

Both name it in the type system: it is not a heuristic inside one field's ingest, it is a property
every drawable carries. **That is what "not immediately visible in the engine" means here** -- the
concept has no name, so it cannot be asked for, measured per subject, or declared by a scenario.

## What will be true

- a level of detail is a NAMED thing the engine holds, and every drawable has one
- the tile cascade the ground already uses is what the OSM geometry hangs from -- one rung coarser
  ground means one step coarser buildings, because they are the same distance away
- the proxy is built WHEN the geometry is built, never in the frame (board:2122)
- a distant building is coarser, never absent

## What will show I was wrong

`world: and the raised geometry` on Shibuya under the 512 MB ceiling for the whole world, and
`sim p99` under 16.7 ms on all eight places. Today: 618.2 MB and 775-2217 ms.


## Looked at, 2026-09-04

`kMassedAtPx = 8` moved 1.07% of OldTown's pixels, 9477 of them by more than 1 of 255, so the rule
applies: LOOK before believing the number. Both images read side by side.

**The foreground is identical** -- the old town's tiled roofs, the church tower, every facade
unchanged. **The change sits on the HORIZON**, which is where it belongs: the far right skyline
carried a scatter of individual bright points before, and now carries fewer, larger, merged
shapes. **Nothing vanished** -- the mass on the horizon is still there, standing as blocks.

That is the distinction the goal draws and the one that decides whether this is LOD or damage:
`Lump` MERGES rather than drops, so a skyline cannot flicker as the camera moves. Measured 1.07%,
and every one of those pixels is at the far distance.
