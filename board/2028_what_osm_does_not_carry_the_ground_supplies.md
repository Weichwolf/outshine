Type: feature
State: open
Parent: 1946
Area: world

# What OSM does not carry, the ground supplies

**Benchmark** — Unreal: an actor placed on a landscape traces down to it and takes that Z -- the surface traced is the one rendered. RAGE: nothing is placed at runtime, because a map entity's Z is baked at export; the runtime's downward probe (`WorldProbe`, against a `phBound`) answers the COLLISION surface, which is a coarser body than the drawn mesh and may sit a metre off it. **Taking Unreal** -- and the reason is that this tree HAS no export step to bake into, so the height has to be right at the moment of drawing, against the surface that is drawn. RAGE's answer is right for a world authored offline and says nothing about one generated on arrival. Cited beside it: Cesium's `sampleHeightMostDetailed` answers the height of the tileset actually loaded rather than of the source raster, which is the same distinction this item is about.

**The owner's words:** OSM knows no height. The buildings must be laid on the landscape, and so must
the streets and rails.

Today `BuildingField::RingBase` takes a base from the `GroundQuery`, which is the raw DEM. The
terrain that is drawn is the cascade's mesh, sampled on a 33 x 33 grid per tile and coarser with
distance -- so the DEM and the drawn surface differ by whatever the grid missed. A building placed
on the DEM sinks into or floats over the terrain a viewer actually sees.

**This is not yet the cause of anything measured.** board:1946's invisible buildings turned out to
be a near plane, and their heights were right all along: the ring within 3.2 km ran 318 to 460 m and
the buildings 329 to 489 m. So this item is filed on the PRINCIPLE and its own measurement, not on a
symptom.

## What will be true

- [ ] a footprint's base is the height of the terrain that is DRAWN beneath it. The buildings cover the same gap with a PLINTH instead -- derived from the footprint's own ground spread -- which works and costs geometry a drape would not
- [x] a way's centreline asks the ground that is DRAWN at every vertex. Measured at Rothenburg: the drawn ground stood over a road by 11 m at worst and under a metre on average; it now stands over none, and the street network is visible near and far
- [ ] the query is a RAY against the mesh rather than a grid cell's high point -- the stand-in's error is a cell's own relief, and the cell is the tile grid's 32 m
- [ ] when the terrain's level changes under a standing building, the building moves with it

## The measurements that would show I am wrong

1. **The gap, per building.** For every footprint, its base against the drawn ring's height at the same east/north. Today that difference is unmeasured; if its largest value is under a metre, the DEM and the drawn surface already agree and this item is not worth its cost
2. **The negative control is a coarse level.** At the cascade's outermost level a tile's grid is kilometres wide, so the gap must be LARGER there. If it is not, the measure is not seeing what it claims
