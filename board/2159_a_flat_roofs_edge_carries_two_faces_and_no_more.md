# A flat roof's edge carries TWO faces and no more

**State:** open · **Lab rank:** 8 · **Waits on:** board:2146 · **Found:** 2026-09-07, measured over Rothenburg's 5 709 bodies

## What is wrong

172 of 2 537 flat-roofed bodies in OldTown's extract are not watertight, and every one of them
fails the same way: **0 open edges and exactly 1 BAD edge** -- one edge carrying more than two
faces. No open edge, no hole, no gap: a face too many.

It is not the crease work and it is not the pitch. The same 172 stand in every configuration
measured, with creases and without:

| roof mesher | open bodies of 5 709 |
|---|---|
| no crease constraint at all | 381 |
| `skeleton` crease (dropped) | 291 |
| `axis` crease only | **198**, of which **172 are flat** |

The 172 are the whole of what is left after the gabled ones were repaired. They share nothing
else: no interior rings (0 in all 172, and 0 in 2 365 of the sound ones either), no `min_height`,
no parts, ordinary 8-corner plans of ordinary area.

## What the two references do

| | |
|---|---|
| **Unreal** | nothing: a roof is a mesh an artist built and the DCC tool welded |
| **RAGE** | the same -- authored, and the pipeline's own check is what catches a third face |
| **id Tech 4** (cited, GPL) | `idSurface` dmap-side: a shared edge is found by matching vertex INDICES after welding, and a third face on an edge is a `LEAK` the compiler refuses to build |

**The choice is id Tech 4's, and it is not really a choice**: a generator has no artist, so the
mesher has to be the one that cannot produce it. The rule already stands in this tree -- `tri()`
counts the faces on every directed edge -- and what is missing is the CAUSE, not the check.

## What will be true

A flat roof's boundary and the wall's top carry the SAME vertices in the SAME order, so no edge
can be walked by three faces. The likely cause to test first: the flat roof's constrained
triangulation is free to place a vertex on the boundary that the eaves band does not have, or the
band and the roof both claim the boundary edge itself.

## The measurement that shows I was wrong

`bad_edges()` over the whole extract, per roof shape. It reads 172 on `flat` today and must read
0; the negative control is the tree as it stands, which reads 172. And the number is quoted from
a REAL extract rather than the 41 synthetic cases, because all 41 are green and always were --
this defect needs a footprint a surveyor drew.
