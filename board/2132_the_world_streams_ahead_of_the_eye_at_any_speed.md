Type: feature
State: open
Area: engine, world, generators
Tags: architecture, performance, owner
Depends: 2122, 2124, 2123

# The world streams AHEAD of the eye at any speed, from a walk to a flight

**Benchmark** -- Unreal's World Partition streams from STREAMING SOURCES that carry a velocity:
the loading shape is pushed along the direction of travel, cells ahead load before cells
behind, and HLOD cells stay resident far out so the horizon never empties. RAGE scales its
streaming radii with the vehicle's speed and keeps SLOD models resident to the horizon -- a
flight over the city sees the whole city as blocks and streams the detail under the aircraft.
**Both agree**: the ring is not centred on the eye, it is ahead of it; the far field is coarse
and RESIDENT, and the fine field is streamed under the velocity vector.

**Cited beside the two**: Cesium's tile scheduler orders requests by SCREEN-SPACE ERROR --
the tile that would change the picture most lands first, whatever its distance -- and its
cache evicts by a memory budget (`cacheBytes`) in order of last use. `TileWatermark::Ask` sorts
by distance today; the pushed centre and the error rule make it Cesium's order.

## Where it stands, measured 2026-09-04

```
  the ring          centred on the eye, kVectorRing = 3 (7x7 vector tiles), recentred whole
  the far field     terrain: a cascade to 393 km   buildings: Massed blocks to awayKm ~6, nothing beyond
  velocity          unknown to the streaming; Restand(at) takes a place and no direction
  the walk          removed from make shots because a tile crossing cost a second (board:2124)
```

The places are deliberately still pictures so preload and frame are measured apart. That is
right for the instrument and says nothing about the engine's aim: outshine is a game engine, and
movement at every place on Earth at every speed -- a walking simulator to a flight simulator --
is what it exists for. Today a walking pace crosses a tile every few minutes and costs a second;
a flight at 250 m/s crosses one every second and the engine stands still.

## The solution

- **the ring is a SHAPE with a direction**: the streaming source carries `at` and `velocity`;
  the wanted set is the ring pushed along the velocity by the distance covered in the time a
  tile takes to land, so a tile is asked for BEFORE the eye needs it. `TileWatermark::Ask`
  already sorts by distance; the distance is measured from the pushed centre
- **the far field is coarse and stays**: `Skyline` pieces (board:2123) for buildings and the
  coarse terrain rungs are resident to the horizon and never evicted by a recentre; only the
  fine ring moves. A recentre by one tile is one fine tile in and one out (board:2124)
- **the budget scales with speed**: the counted budget (board:2105) is placements per frame; at
  speed the rung that is asked for is coarser, so the count of pieces stays and the bytes per
  piece fall. The rule is projected error (board:2123) evaluated at the pushed centre, so
  nothing is invented for speed -- a fast eye is a distant eye
- **the instrument grows a second shape**: `make shots` keeps the still picture and adds a
  WALK and a FLIGHT per place, with the walk digest (board:2105) and the frame budget scored on
  both

## What will be true

- [ ] `make shots` walks and flies every place: 0 of 120 over 16.67 ms at a walk (1.5 m/s) and
      at a flight (250 m/s), and every frame draws a whole world -- no bare tile in the frame
- [ ] The streaming source carries a velocity and the wanted set is pushed along it; a case
      moves the eye at speed and counts tiles that were asked for AFTER the eye reached them,
      which reads 0
- [ ] The far field is resident: a case flies 50 km and the horizon's piece count never drops
- [ ] Negative control: centre the ring on the eye again and the late-tile count at a flight
      goes RED

## What will show I was wrong

If the network cannot land a fine tile in the time a walking eye takes to cross one, no ring
shape helps and the answer is a coarser fine rung at that speed -- measured as the tile's
landing time against the crossing time, per place.
