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


## A THIRD HYPOTHESIS DEAD, AND A REAL DEFECT FOUND BESIDE IT

The fan is not the cause: `RoofSurface::EarClip` is a proper ear clipper, not a fan, so a concave
footprint is triangulated correctly. And a 309 m triangle turns out to be EXPLICABLE -- `Walls`
raises a quad per ring edge, and Rothenburg's town wall is one closed way of about 2.5 km, so a
309 m wall segment is a 309 m triangle and belongs there. The reach census names real geometry as
often as it names a defect, which is what its own page must say.

**What the clipper does when it cannot find an ear is the defect:**

    if (!cut) return;

It stops and leaves the polygon PARTIALLY triangulated -- some ears cut, the rest of the surface
simply absent. Nothing counts it and nothing says so. That is the open faces the owner saw, and it
is a silent refusal of exactly the kind this tree keeps finding: `Grows()` had eight of them,
`Geometry` had three thrown away with a `(void)`.

Its guard is `n * n + 8` iterations, so a ring of 512 points may also simply run out.

## What will be true

- [ ] a clipper that cannot finish SAYS SO, and the count of surfaces it left open is published
- [ ] a footprint whose ring it cannot clip is refused whole rather than half-drawn
- [ ] the count is zero at all five places, or the rings that defeat it are named

## The measurement that would show I am wrong

1. **Count the bail-outs.** If `EarClip` never hits `if (!cut) return;` at any of the five places,
   the open faces come from somewhere else and this is misfiled. That number does not exist today,
   which is the whole point


## THE CLIPPER GIVES UP, AND IT IS NOT THE SLIVERS EITHER

The count the item asked for now exists, and it is not zero:

    roofs the clipper could not cover    Rothenburg 7    Shibuya 173    Venice 168

So `EarClip` really does bail, and those roofs really were half-drawn. They are refused whole now and
counted, which is 56 triangles fewer at Rothenburg. **The crop is unchanged.** The bail-outs are a
real defect and they are not this one.

## FOUR HYPOTHESES DEAD, AND WHAT THE SLIVERS LOOK LIKE

    the fan                     EarClip is a proper ear clipper
    thinness (area)             counted to 8 while the crop stood still
    reach (309 m)               Rothenburg's town wall is one 2.5 km way; a long wall is a long triangle
    the clipper's bail-outs     counted, refused, crop unchanged

Seen rather than counted: they are STRAIGHT, BRIGHT and ROOF-COLOURED, and they cross neighbouring
roofs and the ground. That is what a roof plane standing nearly VERTICAL looks like -- edge-on it
is a line, and it is lit as a roof because it is one. Not a degenerate triangle: a well-formed
surface in the wrong plane.

## The measurement that would show I am wrong

1. **The pitch of every roof plane.** A roof triangle whose normal lies more than, say, 70 deg off
   vertical is standing on its edge. Count them, and their footprints. If that count is zero the
   slivers are something else again and the fifth hypothesis dies with the first four
2. **The eye stays the control.** Every instrument in this item has agreed with itself and disagreed
   with the frame at least once, so no number closes it without the crop


## FIVE HYPOTHESES DEAD. AT TWICE THE RESOLUTION THEY ARE SURFACES.

    the fan                    EarClip is a proper ear clipper, not a fan
    thinness by area           counted down to 8 while the crop stood still
    reach at 309 m             Rothenburg's town wall is one 2.5 km way -- a long wall IS a long triangle
    the clipper's bail-outs    7 / 173 / 168 counted and refused; crop unchanged
    ALIASING                   the renderer runs at SAMPLECOUNT_1 with no MSAA and a TAA jitter that
                               has nothing to accumulate over two frames, so sub-pixel geometry
                               aliasing was the obvious answer -- and rendering the same frame at
                               2560 x 1440 does not remove them. They get WIDER

At double resolution they resolve into real thin roof SURFACES: they have width, they run along
ridges, and they reach OUT PAST THE FOOTPRINT they belong to. Not slivers, not artefacts -- roof
planes placed outside their own building.

## The measurement that settles it

1. **A roof vertex outside its own ring.** For every roof triangle, whether each of its three
   vertices lies inside the footprint polygon it was meshed from. That count is the defect, and it
   is zero for a correct roof by definition
2. **The negative control is the walls.** They are built from the same ring and must read zero
   already; if they do not, the test is wrong rather than the roofs

## What is NOT the finding, and the tree should not forget it

The renderer has no anti-aliasing at all -- `SDL_GPU_SAMPLECOUNT_1` everywhere, and `Jitter_` exists
for a TAA that nothing accumulates. That is a real gap against both references, and it is filed
where it belongs rather than here, because this defect survives at twice the pixels.
