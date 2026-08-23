#ifndef OUTSHINE_GROUND_TILES_TERRAINTILES_H
#define OUTSHINE_GROUND_TILES_TERRAINTILES_H

#include <cstdint>
#include <vector>

#include "TerrainGrid.h"
#include "TileGeodesy.h"

namespace outshine::Ground {

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

class TerrainSource {
 public:
  virtual ~TerrainSource() = default;
  [[nodiscard]] virtual TerrainBytes Take(int z, uint32_t x, uint32_t y) = 0;
};

class TerrainTiles {
 public:
  struct Config {
    uint32_t Stride = 1;

    int DemCacheTiles = 0;
  };

  TerrainTiles(TerrainSource &source, EnuFrame frame, Config config);

  TerrainGrid StitchedGrid(int z, uint32_t x, uint32_t y);

  TerrainMesh MeshOf(int z, uint32_t x, uint32_t y);

  uint32_t Stride() const { return Config_.Stride; }

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

  TerrainGrid RawGrid(int z, uint32_t x, uint32_t y);

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

}
#endif
