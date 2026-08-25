Type: feature
State: open
Area: ground, generators
Tags: scope, osm, plausibility

# A reconstructed third dimension is plausible four ways

OSM does not carry the third dimension. Bridges, ramps, over- and underpasses, tunnels and every
other 3D course are RECONSTRUCTED, and what a reconstruction owes is four kinds of plausibility:
**geometric** (it closes, it is continuous, it does not intersect itself or what it crosses),
**physical** (a vehicle can drive it at the speed the class implies), **static** (it stands:
spans, piers and clearances that could carry their own load), **architectural** (it looks like
the thing it is). A guess that holds all four is right; one that holds three is a finding.

At HEAD the tree does none of it: `src/ground/StreetField.cpp:32` counts a tunnel and drops it,
`src/ground/WaterField.cpp` does the same, and no source reads `bridge`, `layer` or `level` at
all. Every crossing in the world is a road drawn flat through whatever it should have gone over
or under.

## What will be true

- [ ] Every reconstructed structure is judged against the four plausibilities, each as a number,
      and the judgement rides with the geometry.
- [ ] The tags are EVIDENCE and never authority: topology first, a crossing with no shared node
      second, `bridge`/`tunnel`/`layer` as a hint, `highway=*` for the limits, the DEM as coarse
      ground — weighted, never trusted.
- [ ] Its three slices close: the elevation solve (board:1500), the earthworks (board:1505) and
      the structures themselves (board:1506).
