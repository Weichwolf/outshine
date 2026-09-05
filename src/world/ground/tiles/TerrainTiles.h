#ifndef OUTSHINE_WORLD_GROUND_TILES_TERRAINTILES_H
#define OUTSHINE_WORLD_GROUND_TILES_TERRAINTILES_H

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "Address.h"
#include "TerrainGrid.h"
#include "TileGeodesy.h"

namespace outshine::Ground {

class TerrainBytes {
public:
  enum class State { Delivered, Deferred, NoTile, Refused };

  static TerrainBytes From(Data::TileId at, std::vector<uint8_t> png) {
    TerrainBytes b(State::Delivered);
    b.At_ = at;
    b.Png_ = std::move(png);
    return b;
  }

  static TerrainBytes Waiting() { return TerrainBytes(State::Deferred); }

  static TerrainBytes Nothing() { return TerrainBytes(State::NoTile); }

  static TerrainBytes Wire() { return TerrainBytes(State::Refused); }

  [[nodiscard]] State Where() const { return Where_; }

  [[nodiscard]] std::optional<std::pair<Data::TileId, std::vector<uint8_t>>> Take() {
    if (Where_ != State::Delivered) { return std::nullopt; }
    return std::pair{At_, std::move(Png_)};
  }

private:
  explicit TerrainBytes(State where) : Where_(where) {}

  State Where_;
  Data::TileId At_;
  std::vector<uint8_t> Png_;
};

class TerrainSource {
public:
  virtual ~TerrainSource() = default;
  [[nodiscard]] virtual TerrainBytes Take(Data::TileId at) = 0;
};

void FillNodeHeights(const TerrainField &field,
                     uint32_t rowPostings,
                     uint32_t colPostings,
                     int nodes,
                     std::vector<float> *out);

class TerrainTiles {
public:
  struct Config {
    uint32_t Stride = 1;

    int DemCacheTiles = 0;
    size_t DemCacheBytes = 0;
    size_t StitchedFieldBytes = 0;
  };

  struct Shaped {
    std::string Kind;
    double AmplitudeM = 0.0;
    double WavelengthM = 0.0;
    double Gradient = 0.0;
    double BearingDeg = 0.0;
    double FocusLatDeg = 0.0;
    double FocusLonDeg = 0.0;
    uint64_t Seed = 0;
  };

  TerrainTiles(TerrainSource &source, EnuFrame frame, Config config);

  void Shapes(const Shaped &how) { Shape_ = how; }

  [[nodiscard]] bool IsShaped() const noexcept { return !Shape_.Kind.empty(); }

  [[nodiscard]] double ShapedAslM(LongitudeLatitude at) const noexcept;

  TerrainGrid StitchedGrid(int z, uint32_t x, uint32_t y);

  [[nodiscard]] std::shared_ptr<const TerrainField> StitchedField(int z, uint32_t x, uint32_t y);

  TerrainGrid::State
  NodesOf(Data::TileId of, int grid, std::vector<float> *out, uint32_t *postings, int *side);

  [[nodiscard]] uint32_t Stride() const { return Config_.Stride; }

  [[nodiscard]] size_t HeapBytes() const;

private:
  Shaped Shape_;

  enum class Side { West, East, North, South };
  enum class Corner { NorthWest, NorthEast, SouthWest, SouthEast };

  struct CacheEntry {
    uint64_t Seq = 0;
    bool Used = false;
    Data::TileId Of;
    TerrainField Field;
  };

  TerrainGrid RawGrid(Data::TileId of);

  struct StitchedEntry {
    uint64_t Seq = 0;
    Data::TileId Of;
    std::shared_ptr<const TerrainField> Field;
  };

  [[nodiscard]] TerrainGrid::State
  StitchCorner(TerrainField &self, float selfRawM, Data::TileId of, Corner corner);

  [[nodiscard]] TerrainGrid::State
  StitchEdge(TerrainField &self, int z, uint32_t nx, uint32_t ny, Side side);

  const TerrainField *CacheLookup(Data::TileId of);
  void CacheStore(Data::TileId of, const TerrainField &field);

  TerrainSource &Source_;
  EnuFrame Frame_;
  Config Config_;
  std::vector<CacheEntry> Cache_;
  std::vector<StitchedEntry> Stitched_;
  uint64_t Seq_ = 0;
};

} // namespace outshine::Ground
#endif
