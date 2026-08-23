Type: bug
Area: test
Tags: mirror, tiles, board-honesty

**The tiles suite covers what its own item named**

board:1745's "What will be true" listed four behaviours for `test/unit/ground/tiles`:
stitch symmetry, crop determinism, **LRU eviction under `kDemCacheCeiling`**, and the
**WGS84 round-trip including the near-pole altitude arm**. The suite that closed it
(`AStitchedEdgePairsPostingsOfTheSamePlace.cpp`) holds the first two. The closing note
disposes of the other two like this:

> Residue for the board: the LRU eviction and the WGS84 near-pole arm the item also names
> are pinned by 1611's inventory, not by this suite

`board/active/1611_the_engine_knows_no_earth.md` is the Earth-constants audit. Grepped:
it names `TileGeodesy.h`'s `kEarthRadiusM`/`kWgs84A` constants (lines 20, 114, 123-125) and
nothing else -- no cache, no eviction, no `EcefToGeoWgs84`, no polar arm. The residue was
handed to an item that does not carry it, so it is carried by nobody.

Both behaviours are live and untested:

- `TerrainTiles::CacheLookup`/`CacheStore` (src/ground/tiles/TerrainTiles.cpp:37-60): a
  linear scan over up to `kDemCacheCeiling = 128` slots with a `Seq_` LRU victim choice,
  called four to five times per `MeshOf`. Nothing asserts that the victim is the least
  recently used one, that a re-store of a live key does not duplicate it, or that
  `HeapBytes()` tracks what is held.
- `EcefToGeoWgs84` (src/ground/tiles/TileGeodesy.cpp:117 `g.AltM = pxy / clat - N`): only
  the EXACT pole is guarded (:102 `if (pxy < 1e-9)`). At 89.99 deg both `pxy` and `clat`
  are near zero and the altitude is a ratio of two vanishing quantities; the conditioned
  form (`p.Z / slat - N * (1 - e2)`) is the standard alternative and nothing chooses
  between them because nothing measures.
- `src/ground/tiles/TileGeodesy.cpp` is one of the 30 src/*.cpp files (of 147) that no
  test names at all.

What will be true:

1. `test/unit/ground/tiles/` holds an LRU case (fill past the ceiling, prove the victim
   and prove a re-store does not duplicate) and a WGS84 round-trip case sweeping latitude
   to 89.999 deg with a stated altitude bound and its derivation.
2. No closing note again hands a residue to an item that does not name it -- when a
   demand is dropped, the note says DROPPED and why, or it files the successor.
