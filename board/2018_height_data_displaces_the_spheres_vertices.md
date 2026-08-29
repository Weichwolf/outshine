Type: feature
State: open
Area: world
Tags: measured

# Height data displaces the sphere's vertices

**Benchmark** — Unreal: a `ULandscapeComponent` samples a heightmap texture and displaces along the component's up. RAGE: a heightfield sector, the same operation on a flat grid. **They agree**, so the matter is closed: displacement is along the LOCAL UP at the vertex, which on a sphere is the geodetic normal and not a constant axis.

STEP TWO OF THREE. 2017 puts the tiles on a sphere with no elevation; this raises each vertex off
it. A vertex at (lon, lat) with raster height h becomes `GeoToEcef(lon, lat, h)` -- the height is
along the geodetic up at that vertex, which is the whole reason the sphere had to be right first.

## What will be true

- [ ] a tile's vertex carries its raster height, displaced along its own geodetic up
- [ ] the ring's relief matches the terrain's, sampled independently: the Grand Canyon reads its rim-to-river drop, Venice reads metres, the Feldberg reads hundreds
- [ ] the coarser ring levels displace from the COARSER raster, so a level's error is bounded by its own zoom rather than by the finest

## The measurements that would show I am wrong

1. `sampleHeight` at the frame's centre and the ring's nearest vertex must agree to within the tile's own grid spacing -- two independent readers of one raster
2. With elevation ON, relief must exceed 2017's curvature-only number by the terrain's actual range; with it OFF, it must fall back to exactly 2017's. Both directions, or the switch is not the thing being measured
3. A tile boundary must not step: the shared edge of two adjacent tiles must read the same height from both sides, to the raster's precision
