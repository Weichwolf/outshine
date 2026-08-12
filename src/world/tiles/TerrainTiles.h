/* THE TERRAIN TILE PIPELINE: bytes from a source, decoded, cropped from the parent past the source's
 * last zoom, edge-averaged against the four neighbours, and handed on as a height field or as the
 * tile's ENU node grid.
 *
 * ONE INSTANCE PER THREAD. It holds a decoded-DEM cache that the tile being built writes, so sharing
 * one would need a lock around the whole build and the pool would be a queue. */
#ifndef TERRAINTILES_H
#define TERRAINTILES_H

#include <cstdint>
#include <vector>

#include "TerrainGrid.h"
#include "TileGeodesy.h"

namespace outshine::World {

/* WHERE THE BYTES OF ONE TERRAIN TILE COME FROM. Empty means "no tile" — absent, or not fetched yet;
 * which of the two it is belongs to the source, not here. */
class TerrainSource {
 public:
  virtual ~TerrainSource() = default;
  virtual std::vector<uint8_t> TakeTerrainPng(int z, uint32_t x, uint32_t y) = 0;
};

class TerrainTiles {
 public:
  struct Config {
    /* Requests above it step up to the parent tile and crop; 0 = every zoom is served. */
    int SourceMaxZoom = 0;
    uint32_t Stride = 1;
    /* Decoded source grids held against the next stitch; 0 = the built-in ceiling. A context that
     * only asks about single tiles pays 256 KiB per slot for a cache it cannot use. */
    int DemCacheTiles = 0;
  };

  /* The source is a reference and the frame is a value: neither "no source" nor "the frame was never
   * built" has a spelling here, which is what the two config failure codes used to be. */
  TerrainTiles(TerrainSource &source, EnuFrame frame, Config config);

  /* The stitched height field of one tile. */
  TerrainGrid StitchedGrid(int z, uint32_t x, uint32_t y);
  /* The same field as this tile's node grid in ENU metres. */
  TerrainMesh MeshOf(int z, uint32_t x, uint32_t y);

  uint32_t Stride() const { return Config_.Stride; }
  /* Everything this instance holds. One per thread, so a pool's whole DEM cache is this summed over
   * its threads — and until it is summed it is a pool the ledger cannot see. */
  size_t HeapBytes() const;

 private:
  enum class Side { West, East, North, South };

  struct CacheEntry {
    uint64_t Seq = 0;
    bool Used = false;
    int Z = 0;
    uint32_t X = 0, Y = 0;
    TerrainField Field;
  };

  /* WITHOUT stitching: splitting raw from stitched is what avoids unbounded recursion, since the
   * stitch pass reads its four neighbours raw. */
  TerrainGrid RawGrid(int z, uint32_t x, uint32_t y);
  /* SYMMETRIC averaging: both sides land on the same midpoint, so the heights line up exactly at the
   * seam. A neighbour that is absent or will not decode costs this edge its averaging and nothing
   * more — that tile reports its own decode failure when it is built. */
  void StitchEdge(TerrainField &self, int z, uint32_t nx, uint32_t ny, Side side);

  const TerrainField *CacheLookup(int z, uint32_t x, uint32_t y);
  void CacheStore(int z, uint32_t x, uint32_t y, const TerrainField &field);

  TerrainSource &Source_;
  EnuFrame Frame_;
  Config Config_;
  std::vector<CacheEntry> Cache_;
  uint64_t Seq_ = 0;
};

}  // namespace outshine::World
#endif
