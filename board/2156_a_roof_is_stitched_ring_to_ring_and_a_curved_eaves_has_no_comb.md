# A roof is stitched RING TO RING, and a curved eaves has no comb

**State:** open · **Waits on:** nothing · **Found:** 2026-09-06, by looking at the roof gallery

## What is wrong

Every roof whose surface is steep at the eaves -- dome, onion, barrel, mansard, gambrel,
half-hipped -- carries a row of SPIKES along the eaves and, on the onion, over the whole bulb. The
body is watertight, every directed edge has its partner, the volume is positive, no two vertices
are unwelded, and the case is GREEN. A viewer rejects it in a second.

## What the measurement says, and what it says was NOT the cause

Two causes were already written into the file as fixed. Both were hypotheses and both were wrong:

| written cause | measured 2026-09-06 |
|---|---|
| `buffer(-d)` is approximate, so a ring point's height is off by epsilon | remembered d vs geometric d: max 0.00000 m; z from d vs z geometric: max 0.00001 m |
| the ring ladder steps in DISTANCE where it should step in height | it stepped in neither: `crown(d)` probed `representative_point()`, a point in the MIDDLE of the offset, which for a dome is already the apex. It returned 2.8008 for every ring, the ladder read a rise of 0.0000 between all of them and never once halved its step |

The cause is the point set's ANISOTROPY. Points stand `cell` apart ALONG a ring and the rings were
0.008 to 0.16 m apart across. A Delaunay triangulation of a 60:1 anisotropic set does not make a
strip -- it makes slivers that SKIP rings. Measured on a 12 x 8 rectangle under a dome, the worst
faces span d = 0.000, 0.008 and 0.164 while the rings at 0.039 and 0.102 stand unused between
them, and those faces sit 53 degrees off the field's own normal.

Three repairs are already in: `crown` measures a point ON the ring, the ladder is floored at the
along-ring spacing so the set stays isotropic, and `Building._build` resets instead of appending
(called twice it left two copies of every face, 4056 edges with more than two faces and exactly
twice the volume). They take the worst faces from 53 to about 20 degrees and the onion's p95 from
30.9 to 20.6. **The comb is still there**, because at a dome's springing the surface really is
near-vertical and a Delaunay over rings alternates its triangles' azimuth there whatever the
spacing.

## What will be true

A roof is meshed as a STRUCTURED ring-and-spoke surface: ring k and ring k+1 carry the same
spokes, in the same order, and the band between them is a quad strip. No triangle spans two bands,
no triangle skips a ring, and the azimuth of a band's faces does not alternate. Where an offset
splits into several parts the split is handled explicitly, at the ring where it happens.

Delaunay stays for the FLOOR, which is a constrained triangulation of a polygon with holes and is
exactly what Shewchuk's `triangle` is for. The roof is not that problem.

## What Unreal does, what RAGE does

Neither ships a roof generator; this is a question neither faces, and the item says so. The
readable body that does is **CGAL's straight skeleton and its `extrude`**, which builds a roof as
faces of the skeleton -- right for a hipped roof and silent about a dome. **Cesium's
quantized-mesh** is the reference for the other half: a surface over a footprint, stitched in
rings with an explicit edge order, which is what makes its tiles join. The choice is mine and the
reason is the measurement above: a height field over a ring ladder is a PARAMETRIC surface, and a
parametric surface is stitched, never triangulated by proximity.

## The measurement that shows I was wrong

`test/lab/buildings/gallery.py` prints, per shape, the angle between each roof face's normal and
the height field's own normal at that face's centroid, and the ratio of the meshed roof area to
the field's own integral. Today, with the three repairs in:

    shape        area ratio   normal p95
    dome              1.142         15.8
    onion             1.265         20.6
    mansard           1.040         21.8
    gabled            1.022          0.0
    hipped            1.022          0.8

The bar is **p95 under 5 degrees on every smooth shape and the area ratio under 1.05**, with
`gabled` and `skillion` staying at exactly 0.0 (they are planar and a correct mesher represents
them exactly). The negative control is the stitch replaced by the Delaunay, which must go red.
And the picture is looked at, because the number 1.142 did not say "a row of spikes".
