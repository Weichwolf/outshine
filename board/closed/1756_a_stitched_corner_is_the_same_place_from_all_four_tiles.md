Type: bug
Area: ground
Tags: seam, tiles

**A stitched corner is the same place from all four tiles**

board:1746 closed the EDGES. The corners are still open, and the proving test says so
rather than fixing it:

    test/unit/ground/tiles/AStitchedEdgePairsPostingsOfTheSamePlace.cpp:51-56
      // the corner postings belong to TWO seams ... and on the coarser side that
      // contamination reaches one COARSE spacing inward. The shared edge proper is what
      // lies past it
      const double margin = 1.0 / (double)(std::min(l->Rows(), r->Rows()) - 1u);
      ... if (along < margin + 1e-9 || along > 1.0 - margin - 1e-9) { continue; }

With 17 postings against 9, margin = 1/8: a full quarter of the shared edge (an eighth at
each end) is excluded from the only proof the seam has.

The cause is in the stitcher, at `src/ground/tiles/TerrainTiles.cpp:138-141`. The four
edges are applied in sequence over the SAME field, so the North pass overwrites what the
West pass just wrote at the shared posting, and the diagonal neighbour is never consulted:

    A.corner = 0.5*( 0.5*(A + East) + North )  = 0.25 A + 0.25 East + 0.5 North(x,y-1)
    B.corner = 0.5*( 0.5*(B + West) + North )  = 0.25 A + 0.25 B    + 0.5 North(x+1,y-1)

Two tiles that share the corner keep two different heights whenever their north
neighbours' raw samples differ -- which is exactly the resolution boundary 1746 exists
for. Same class as 1746: the stitcher makes the seam it exists to close, one posting
further along.

What will be true:

1. A corner posting is the average over ALL tiles that share it (the two edge neighbours
   and the diagonal), computed once, not the composition of two sequential edge passes.
2. `AStitchedEdgePairsPostingsOfTheSamePlace` drops its `margin` exclusion and compares
   EVERY posting of the shared edge including the two ends, plus a case that reads the
   same corner from all four tiles and asserts one height. The negative control is the
   step the sequential passes leave today.

---

Closed -- a corner is ONE average over the four RAW fields that share it, computed after
the edges from the four numbers every one of those tiles sees: StitchCorner takes self's
corner as it stood BEFORE the edge passes (captured in StitchedGrid) and reads the three
neighbours raw, so whichever tile asks gets the same height; at the map's rim, where the
three neighbours do not exist, the tile's own posting stands, which is what an unshared
corner is. The proof dropped its margin: EVERY posting of the shared edge is now compared,
corners included (17 of 17 at a 17-vs-9 boundary), and a new arm reads the shared corner
from all four tiles. Discriminating fixture: one tile served from a DIFFERENT VINTAGE
(+300 m), because with tiles that sample one world exactly the corners agree by
construction and prove nothing -- that is why the first fixture passed against the defect
and had to be made honest. Negative controls: no corner pass at all = 150 m of
disagreement; the corner pass reading self AFTER the edges = 93.75 m; both red.
