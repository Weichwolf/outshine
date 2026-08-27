Type: feature
State: open
Area: ground, generators
Tags: scope, osm, plausibility, measured

# A reconstructed third dimension is plausible four ways

**Benchmark** — Neither engine reconstructs a third dimension: both take bridges and tunnels as authored. **The choice is mine** — geometric, physical, static and architectural plausibility is the bar, because there is no vendor to check it against.

OSM does not carry the third dimension. Bridges, ramps, over- and underpasses, tunnels and every
other 3D course are RECONSTRUCTED, and what a reconstruction owes is four kinds of plausibility:
**geometric** (it closes, it is continuous, it does not intersect itself or what it crosses),
**physical** (a vehicle can drive it at the speed the class implies), **static** (it stands:
spans, piers and clearances that could carry their own load), **architectural** (it looks like
the thing it is). A guess that holds all four is right; one that holds three is a finding.

## What became true at b4adb48d and ab3126d6

The vector reader decoded six of the seven Mapbox `Value` types; field 7 is `bool_value`, so
every way in the tree reported `bridge = 0` and `tunnel = 0`. Both now decode, and both reach
topology:

    src/world/ground/RoadHarvest.cpp:77   into.Lay(..., bridge > 0.5 || tunnel > 0.5)
    src/base/spatial/Wayfinding.cpp:113   size_t Network::Cross() skips a crossing where either way Spans

Measured on the shipped f31 network: 6340 crossings in plan, **5729 joined at grade, 611 left
alone** because one way spans. The graph went 4193 pieces -> 284 and the largest component
59 % -> 96.8 %. The schema carries no `layer` key at all, so those two booleans are the whole of
the third dimension the data offers.

## What is STILL flat, and it is the whole item

`Spans` changes TOPOLOGY and nothing else. A bridge and the road beneath it still lie in one
plane: no clearance is reserved, no deck is lifted, no portal is cut, no pier stands. Geometric
plausibility says a reconstruction *does not intersect what it crosses*, and 611 places in this
one extract do exactly that — they are now correctly refused a junction while remaining
coincident in space, which is a topology that is right about a geometry that is wrong.

`src/world/ground/StreetField.cpp:30` still counts a tunnel and drops it; `WaterField.cpp` does
the same.

## What will be true

- [ ] A way that spans is LIFTED and a way that tunnels is SUNK, by a clearance the class
      declares, and the elevation solve (board:1500) carries the constraint rather than a
      constant. Proving case: a bridge and the road it crosses are vertically separated by at
      least the declared clearance at the crossing point; negative control, the flat graph, and
      the separation is 0.
- [ ] Every reconstructed structure is judged against the four plausibilities, each as a number,
      and the judgement rides with the geometry.
- [ ] The tags are EVIDENCE and never authority: topology first, a crossing with no shared node
      second, `bridge`/`tunnel` as a hint, `highway=*` for the limits, the DEM as coarse ground —
      weighted, never trusted.
- [ ] Its three slices close: the elevation solve (board:1500), the earthworks (board:1505) and
      the structures themselves (board:1506).
