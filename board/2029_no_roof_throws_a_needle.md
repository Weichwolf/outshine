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


## BLOCKED BY board:2026, and the numbers say so

    ring tidied           604 309 triangles    13 needles
    area cutoff 0.01 m2   582 147              8
    aspect cutoff 1e-4    601 897              760

The third line is not a regression in the cut -- it is the population moving. The same place run
three times with unchanged code gives 604 309, 582 147 and 601 897 triangles, because
`BuildingField::Verts_` grows by a variable number of rebuilds. Until board:2026 bounds it, a needle
count cannot be compared with itself.

**The aspect cut STAYS**, and its justification is geometric rather than measured: a needle is
area against its own longest edge, an equilateral triangle carries 0.433 and a sliver tends to zero
whatever its size, so refusing below 1e-4 leaves a triangle 4 000 times thinner than equilateral
standing. The area cutoff it replaced took 22 162 triangles out of Rothenburg to remove 5 slivers --
window mullions and cornices are small AND well-shaped, and a cleaner that eats them is the next
defect. No tick is claimed for either until the count holds still.
