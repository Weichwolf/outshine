Type: bug
Area: test
Tags: mirror, ground, tiles

# The tiles' mirror excuse is true, or the tiles are tested

The mirror gate passes src/ground/tiles on a lie.
test/harness/claims/EverySourceLayerHasItsUnitMirror.cpp:16 excuses the layer as "folded
into unit/world -- unit/world compiles them in its groups", and test/unit/world does not
exist (the directory died in the world→ground rename; the excuse never followed). A grep
over test/ finds NO test naming TerrainTiles, TerrainGrid, TileGeodesy or TileEnuMap —
unit/core/PlanarGeodesyHoldsToItsScope.cpp covers src/core geodesy, not this layer. Four
source files (TerrainGrid.cpp, TerrainTiles.cpp, TileGeodesy.cpp, TileMath.h) carry zero
regression protection while the claims test reports the mirror whole.

What will be true:

1. test/unit/ground/tiles exists and pins the layer's behaviour: stitch symmetry (both
   neighbours converge to the same edge value), crop determinism (the subDiv path at
   TerrainTiles.cpp:89-100), LRU eviction under kDemCacheCeiling, WGS84 round-trip
   Geo→Ecef→Geo including the near-pole altitude arm (TileGeodesy.cpp:119 divides by
   cos(lat) with only the exact-pole case guarded at :102).
2. The claims test refuses an excuse whose named target directory holds no test — a
   stale excuse must go red, not NOTE-green. Today it prints the lie and passes.

---

Closed -- test/unit/ground/tiles exists (AStitchedEdgePairsPostingsOfTheSamePlace pins the
stitch across a resolution boundary, the crop path that manufactures it, and the mesh's
height reading), the tiles layer's excuse is GONE from the claims list, and the claims test
now refuses a stale excuse: every folding excuse names its target suite and a target that
holds no test goes RED instead of printing the lie in green. Negative control: pointing the
core/io excuse at test/unit/nowhere turns the claims test FAIL. Residue for the board: the
LRU eviction and the WGS84 near-pole arm the item also names are pinned by 1611's
inventory, not by this suite -- named here so nobody reads the suite as complete.
