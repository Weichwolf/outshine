Type: bug
Area: ground
Tags: tiles, terrain, seam

# A stitched edge pairs postings of the same place

TerrainTiles::StitchEdge (src/ground/tiles/TerrainTiles.cpp:106-126) averages edge
postings index-by-index and truncates with std::min(rows). That is only correct when both
fields carry the SAME posting count along the shared edge. The crop path directly above it
(TerrainTiles.cpp:89-100) manufactures the other case: a tile whose bytes come from a
coarser ancestor decodes to Cols/subDiv postings — so at exactly the boundary where one
tile is served native and its neighbour is served cropped, the loop pairs posting r of a
fine edge with posting r of a coarse edge. Two failures at once:

- the paired postings are different geodetic points (fine spacing vs coarse spacing), so
  the average moves the edge toward a height that belongs elsewhere;
- min() leaves the fine edge's remaining rows untouched, so half the edge is stitched and
  half is not — a step ALONG the edge that the unstitched picture did not have.

The stitcher creates the seam it exists to close, precisely at resolution boundaries.

What will be true: StitchEdge resamples the neighbour's edge by fractional position along
the shared edge (the same PostingFrac currency TerrainMesh::Over already uses), covers ALL
of self's edge postings, and a unit test (twin of 1745's suite) proves that a native tile
beside a cropped one meshes with a closed seam — matching heights at every shared
fraction, both directions.

---

Closed -- StitchEdge pairs by PLACE: the fraction along the shared edge is the currency,
the neighbour's edge is read with the posting reader (TerrainField::PostingM, added in the
same repair because InterpolatedM addresses texel centres -- board:1750), and EVERY posting
of the finer side is covered, so nothing is left truncated. Proven in
AStitchedEdgePairsPostingsOfTheSamePlace: a native 17-posting tile beside a 9-posting one
meets at 0 m across the whole shared edge past the corner margin (the corners belong to two
seams by design); negative control: the index-paired stitcher answers 187.5 m of gap on the
same fixture.
