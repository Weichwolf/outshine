#ifndef OUTSHINE_WORLD_GROUND_TILES_TERRAINTILES_H
#define OUTSHINE_WORLD_GROUND_TILES_TERRAINTILES_H

#include <cstdint>
#include <memory>
#include <map>
#include <mutex>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "Address.h"
#include "TerrainGrid.h"
#include "TileSourceIdentity.h"
#include "TileGeodesy.h"

namespace outshine::Ground {

class TerrainBytes {
public:
  enum class State { Delivered, Deferred, NoTile, Refused };

  struct Payload {
    Data::TileId At;
    std::vector<uint8_t> Png;
    Data::TileSourceIdentity Source;
  };

  static TerrainBytes
  From(Data::TileId at, std::vector<uint8_t> png, Data::TileSourceIdentity source) {
    TerrainBytes b(State::Delivered);
    b.Payload_ = Payload{.At = at, .Png = std::move(png), .Source = std::move(source)};
    return b;
  }

  static TerrainBytes Waiting() { return TerrainBytes(State::Deferred); }

  static TerrainBytes Nothing() { return TerrainBytes(State::NoTile); }

  static TerrainBytes Wire() { return TerrainBytes(State::Refused); }

  [[nodiscard]] State Where() const { return Where_; }

  [[nodiscard]] std::optional<Payload> Take() {
    if (Where_ != State::Delivered) { return std::nullopt; }
    Where_ = State::Deferred;
    return std::move(Payload_);
  }

private:
  explicit TerrainBytes(State where) : Where_(where) {}

  State Where_;
  Payload Payload_;
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

class DecodedCache {
public:
  explicit DecodedCache(size_t budgetBytes) : Budget_(budgetBytes) {}

  [[nodiscard]] bool Take(Data::TileId of, TerrainField *out);
  void Store(Data::TileId of, const TerrainField &field);
  [[nodiscard]] size_t Bytes() const;

private:
  struct Entry {
    uint64_t Seq = 0;
    Data::TileId Of;
    TerrainField Field;
  };

  mutable std::mutex Lock_;
  std::vector<Entry> Held_;
  size_t Budget_ = 0;
  uint64_t Seq_ = 0;
};

class TerrainTiles {
public:
  struct Config {
    uint32_t Stride = 1;

    size_t DemCacheBytes = 0;
    std::shared_ptr<DecodedCache> Shared;
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

    [[nodiscard]] bool operator==(const Shaped &) const noexcept = default;
  };

  TerrainTiles(TerrainSource &source, EnuFrame frame, Config config);

  void Shapes(const Shaped &how) {
    if (Shape_ == how) { return; }
    Shape_ = how;
    Stitched_.clear();
    StitchedBytes_ = 0;
  }

  [[nodiscard]] bool IsShaped() const noexcept { return !Shape_.Kind.empty(); }

  [[nodiscard]] double ShapedAslM(LongitudeLatitude at) const noexcept;

  TerrainGrid StitchedGrid(int z, uint32_t x, uint32_t y);

  [[nodiscard]] std::shared_ptr<const TerrainField> StitchedField(int z, uint32_t x, uint32_t y);

  [[nodiscard]] std::shared_ptr<const TerrainField> HeldStitched(Data::TileId of) const;
  void HoldsStitched(Data::TileId of, const std::shared_ptr<const TerrainField> &shared);

  TerrainGrid::State NodesOf(Data::TileId of,
                             int grid,
                             std::vector<float> *out,
                             std::vector<Data::TileSourceIdentity> *sources,
                             uint32_t *postings,
                             int *side);

  [[nodiscard]] uint32_t Stride() const { return Config_.Stride; }

  [[nodiscard]] size_t HeapBytes() const;

private:
  Shaped Shape_;

  enum class Side { West, East, North, South };
  enum class Corner { NorthWest, NorthEast, SouthWest, SouthEast };

  TerrainGrid RawGrid(Data::TileId of);

  struct StitchedEntry {
    uint64_t Seq = 0;
    std::shared_ptr<const TerrainField> Field;
  };

  struct TileIdLess {
    [[nodiscard]] bool operator()(Data::TileId left, Data::TileId right) const noexcept {
      if (left.Zoom != right.Zoom) { return left.Zoom < right.Zoom; }
      if (left.X != right.X) { return left.X < right.X; }
      return left.Y < right.Y;
    }
  };

  [[nodiscard]] TerrainGrid::State
  StitchCorner(TerrainField &self, float selfRawM, Data::TileId of, Corner corner);

  [[nodiscard]] TerrainGrid::State
  StitchEdge(TerrainField &self, int z, uint32_t nx, uint32_t ny, Side side);

  TerrainSource &Source_;
  EnuFrame Frame_;
  Config Config_;
  std::shared_ptr<DecodedCache> Decoded_;
  bool SharesDecoded_ = false;
  std::map<Data::TileId, StitchedEntry, TileIdLess> Stitched_;
  size_t StitchedBytes_ = 0;
  uint64_t Seq_ = 0;
};

}
#endif
