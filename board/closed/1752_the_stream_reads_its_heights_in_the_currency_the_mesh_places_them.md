Type: bug
Area: ground
Tags: currency, dem, regression

**The stream reads its heights in the currency the mesh places them**

board:1750 was repaired in ONE of the two readers of the same field. `TerrainMesh::Over`
(src/ground/tiles/TerrainGrid.cpp:56) now reads `field.PostingM(fc, fr)`. The other
consumer of the SAME `StitchedGrid` did not follow:

    src/ground/TerrainLoader.cpp:57-60
      const double fr = PostingFrac(ChunkNodePosting(j, rowPostings, nodes), rowPostings);
      const double fc = PostingFrac(ChunkNodePosting(i, colPostings, nodes), colPostings);
      (*out)[...] = field.InterpolatedM(fc, fr);      // the TEXEL reading

`PostingFrac` mints the posting currency (`TerrainGrid.h:57`); `InterpolatedM` is declared
in the header itself as "the TEXEL reading: fractions address texel centres". Mixing them
is the whole of 1750, and this is the path that FEEDS the drive: `FillNodeHeights` fills
`Tile::H`, which `TileHeightAslM` reads for `GroundStream::At` -- the height the wheels
stand on. `TerrainMesh::Over` feeds `TilePool.cpp:322 -> ChunkBuildEcef`, the terrain the
camera sees. The two now disagree by construction.

Measured (probe, 256-posting field, linear ramp of 900 m across the tile, stride 1,
grid 64 -> 65 nodes; the two readings differenced node by node):

| quantity | value | origin |
|---|---|---|
| worst stream-vs-mesh disagreement | 1.7232 m | measured, node 1 of 65 |
| ramp per posting | 3.5294 m | derived, 900/(256-1) |
| misregistration | 0.4882 postings | derived, 1.7232/3.5294 |

Half a posting spacing, the same figure 1750 named. Deviation is `p/(Cols-1) - 0.5`
postings at posting p -- a linear STRETCH of the whole field, worst at both ends, zero at
the centre, clamped only exactly at the border.

At z=12, 48 deg N: one posting is 40075016*cos(48)/4096/255 = 25.7 m, so the drawn ground
and the queried ground stand up to 12.5 m apart horizontally.

What will be true:

1. `FillNodeHeights` reads `field.PostingM(fc, fr)` -- ONE currency in the layer, as 1750
   decided.
2. A unit test in `test/unit/ground/` puts a ramp field through BOTH readers and asserts
   they agree at every node: the mesh vertex height at a place equals `GroundStream::At`
   at the same place. That test fails today at 1.72 m and would have caught the split.
3. `src/ground/TerrainLoader.cpp` stops being one of the 30 src files no test names
   (see 1757).

---

Closed -- and the SECOND SPELLING is gone, not merely bypassed: TerrainLoader's
FillNodeHeights reads with PostingM, which left InterpolatedM and TexelIndex with zero
callers, so both are DELETED (delete on the day you replace). A height grid now has exactly
one reading and the texel currency is unspellable -- which is why 1750's repair was
incomplete: it fixed one reader and left the other spelling standing for the next caller to
pick up. Proven in unit/ground/TheStreamAnswersWhereTheMeshPlaces: a 257-posting ramp field
filled to chunk nodes and read back at every interior node and all four corners answers the
ramp AT ITS OWN PLACE (0 m). Negative control: restoring the texel reading inside PostingM
answers 2.65 m away at 64 nodes -- half a chunk-node spacing, as derived. The layer's own
posting maths (FillNodeHeights, TileHeightAslM) is nameable in TerrainLoader.h so the unit
mirror can hold it at all -- the file had no test naming it before this hour.
