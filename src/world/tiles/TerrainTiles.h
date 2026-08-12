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

/* WHAT A SOURCE ANSWERED FOR ONE TERRAIN TILE. Four states, because *no tile here*, *not yet*,
 * *the transport refused* and *an empty body* are four different things and were one empty vector
 * with the reason on a thread-local. The bytes and the address they came FROM are handed over
 * together: a request above the source's last native zoom is answered from an ancestor, and which
 * one it was is what the crop below is computed from. */
class TerrainBytes {
 public:
  enum class State { Delivered, Deferred, NoTile, Refused };

  static TerrainBytes From(int z, uint32_t x, uint32_t y, std::vector<uint8_t> png) {
    TerrainBytes b(State::Delivered);
    b.Z_ = z;
    b.X_ = x;
    b.Y_ = y;
    b.Png_ = std::move(png);
    return b;
  }
  static TerrainBytes Waiting() { return TerrainBytes(State::Deferred); }
  static TerrainBytes Nothing() { return TerrainBytes(State::NoTile); }
  static TerrainBytes Wire() { return TerrainBytes(State::Refused); }

  [[nodiscard]] State Where() const { return Where_; }

  /* The one door: the address that answered cannot be read without the bytes it answered with. */
  [[nodiscard]] bool TryTake(int *z, uint32_t *x, uint32_t *y, std::vector<uint8_t> *png) {
    if (Where_ != State::Delivered) return false;
    *z = Z_;
    *x = X_;
    *y = Y_;
    *png = std::move(Png_);
    return true;
  }

 private:
  explicit TerrainBytes(State where) : Where_(where) {}

  State Where_;
  int Z_ = 0;
  uint32_t X_ = 0, Y_ = 0;
  std::vector<uint8_t> Png_;
};

/* WHERE THE BYTES OF ONE TERRAIN TILE COME FROM. The source owns its own zoom bound: a request above
 * it comes back from an ancestor with that address stated, so this class never has to be told what
 * the upstream's last zoom is. */
class TerrainSource {
 public:
  virtual ~TerrainSource() = default;
  [[nodiscard]] virtual TerrainBytes Take(int z, uint32_t x, uint32_t y) = 0;
};

class TerrainTiles {
 public:
  struct Config {
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
   * seam. Answers with the neighbour's own state, which is what lets the stitch tell "there is no
   * tile there to average with" from "that tile has not arrived yet". */
  [[nodiscard]] TerrainGrid::State StitchEdge(TerrainField &self, int z, uint32_t nx, uint32_t ny,
                                              Side side);

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
