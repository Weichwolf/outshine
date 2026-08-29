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


## THE POPULATION HOLDS STILL, AND board:2026's BLOCK WAS MY OWN ERROR

Rothenburg run twice on identical code: 601 897 triangles, 5 140 footprints, 5 vector tiles --
IDENTICAL. The three numbers that looked like drift (604 309, 582 147, 601 897) came from three
different CODE states, not three runs. I compared apples with oranges and blamed the fruit. board:2026
stands on its own merits and blocks nothing here.

## THE FIRST INSTRUMENT COULD NOT SEE WHAT THE FRAME SHOWS

It counted a needle as area under 0.01 m2 with an edge over 5 m, and drove that to 8 -- while the
crop at eight times magnification shows the slivers UNCHANGED. Measured off the frame: they are
under a pixel wide and about fifty long, so on the order of 0.15 m by 7.5 m -- about 0.56 m2, fifty
times the area that instrument refuses. It was measuring a different thing and reporting success.

## WHAT THEY ACTUALLY ARE: REACH

    triangles reaching over 20 m     15 920
    the furthest any reaches            309 m

A 309 m triangle in a town of 10 m houses belongs to no building. Thinness was never the mark; span
is. The slivers in the crop run diagonally ACROSS other buildings and the ground, which is what a
triangle built from two different rings looks like.

## What will be true

- [ ] no triangle a building hands over spans further than the footprint it belongs to
- [ ] the count over 20 m falls to what the town's genuinely large structures explain -- a church, a barn, a terrace meshed as one -- and the furthest is one of those rather than 309 m
- [ ] the crop at eight times magnification shows no diagonal crossing a neighbour

## The measurements that would show I am wrong

1. **Span against the footprint's own extent**, not against a constant. A triangle wider than the ring it came from is the defect; a 40 m barn is not
2. **The eye is the control the counters failed.** The same crop at the same magnification. A count that reaches zero while the picture still shows them means the measure is not seeing what I am -- which is exactly what happened to the first one
