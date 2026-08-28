Type: bug
State: open
Parent: 1946
Area: generators
Tags: measured

# No roof throws a needle

**Benchmark** — Unreal: a mesh that reaches the renderer has been through a build step that drops degenerate triangles. RAGE: authored geometry is validated at export. **They agree**, so the matter is closed: a needle is a defect at the SOURCE, never something a renderer works around.

SEEN, at Rothenburg, in `build/places/OldTown.png` magnified eight times around (250, 545): long
thin bright slivers shoot diagonally out of roof corners, and some roof planes extend far past the
building they belong to. They are lit like roof surfaces, so they carry roof normals -- a triangle
whose third vertex has run away rather than a stray primitive.

`RoofSurface` fans a footprint's ring, and a ring with a collinear or repeated point produces a
triangle of near-zero area whose normal is meaningless and whose vertices can be projected anywhere.
OSM rings carry both: a way that closes on its first point, and nodes a metre apart on a straight
facade.

## What will be true

- [ ] no triangle a building hands over has an area below a threshold derived from the footprint's own scale
- [ ] a ring is cleaned before it is meshed: repeated points dropped, collinear runs collapsed
- [ ] the count of triangles refused is PUBLISHED, because a cleaner that silently eats geometry is the next defect

## The measurements that would show I am wrong

1. **Count them.** The number of building triangles whose area is under, say, a thousandth of the footprint's own area. If that count is zero, the slivers come from somewhere else and this item is misfiled
2. **The negative control is the count itself.** Cleaning must drive it to zero while the triangle total falls by no more than that same number -- if the total falls further, the cleaner is eating real geometry
3. **And the eye.** The same crop at the same magnification, with no slivers. A count that reaches zero while the picture still shows them means the measure is not seeing what I am
