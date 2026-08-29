Type: feature
State: open
Parent: 1946
Area: world, generators

# Streets, rails and paths are geometry, extruded profiles with their own materials

**Benchmark** — Unreal: a road is a spline mesh with a cross-section swept along it, and its surface is a material like any other. RAGE: roads are authored map geometry with their own shaders. **They agree**, so the matter is closed: a road is GEOMETRY, never a stripe painted on the terrain.

**The owner's words:** streets, rails and paths as real geometry -- extruded profiles with their own
materials. And: OSM knows no height, so they must be LAID on the landscape, exactly as the buildings
must.

The tree already reads them. `StreetField` ingests OSM ways and the places measure 2 159 streets at
Rothenburg, 15 833 at Shibuya. Nothing turns them into geometry, which is the same gap the buildings
had until `BuildingField::Shapes` was given a mesher.

## What will be true

- [x] a way becomes a ribbon: swept along its centreline at the way's own declared half width, with its own material. 1 867 ways at Rothenburg, 5 refused, 15 828 triangles
- [ ] the profile carries a KERB, a camber and a verge -- it is a flat band today, which is the simplest honest cross-section and not the finished one
- [ ] rails and footways get their own profile and material rather than the carriageway's
- [ ] the ribbon follows the LANDSCAPE along its whole length, sampled from the ground that is DRAWN rather than from the raw DEM. MEASURED over 31 275 road vertices at Rothenburg: the drawn ground stands over a road by under a metre on average and by 11 m at worst, and that tail is a grid cell's own relief on a slope rather than a constant error. The ribbon rides 1 m today, which covers the town and does not pretend to cover the hillside -- board:2028 owns the real answer
- [ ] a junction does not leave a hole or a fold
- [ ] the profile is DECLARED, not coded: a scenario states widths and materials, and the engine's default stands where it does not

## The measurements that would show I am wrong

1. **Length, not impression.** The drawn ribbon's total centreline length must equal the sum of the OSM ways' own lengths within the ring, computed from the raw features. A ribbon that misses half the ways reads half
2. **It lies ON the ground.** Sample the ribbon's height against the drawn terrain at a hundred points along it: the largest gap must be under the terrain's own vertex spacing. The negative control is the buildings, which sat correct in height all along and were still invisible for another reason
3. **A profile is a profile.** Its cross-section must be constant where the class is constant -- measure the width at a hundred stations and it must not drift
