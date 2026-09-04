#ifndef OUTSHINE_WORLD_GROUND_TERRAINLOADER_H
#define OUTSHINE_WORLD_GROUND_TERRAINLOADER_H
#include <stdint.h>

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include "ChunkSurface.h"
#include "GroundQuery.h"
#include "GroundSample.h"
#include "TilePool.h"

namespace outshine::Data {
class SourceSet;
class Transport;
} // namespace outshine::Data

namespace outshine::Ground {

constexpr int kStreamGrid = 64;

struct GroundSurface {
  int Z;
  int Grid;
};

class TerrainField;
class TerrainTiles;

void FillNodeHeights(const TerrainField &field,
                     uint32_t rowPostings,
                     uint32_t colPostings,
                     int nodes,
                     std::vector<float> *out);
[[nodiscard]] double
TileHeightAslM(const float *nodes, int side, uint32_t postings, double fx, double fy);

class GroundBlock {
public:
  enum class State { Resolved, Pending, Missing };

  [[nodiscard]] State Where() const noexcept { return Where_; }

  void AslMRow(LongitudeLatitude from, double lonStepDeg, std::span<double> out) const noexcept;

  static GroundBlock Over(const float *nodes, TileSpot at, Sampling raster) {
    GroundBlock out;
    out.Nodes_ = nodes;
    out.Zoom_ = at.Zoom;
    out.X_ = at.X;
    out.Y_ = at.Y;
    out.Side_ = raster.Side;
    out.Postings_ = raster.Postings;
    out.Where_ = nodes != nullptr ? State::Resolved : State::Missing;
    return out;
  }

  static GroundBlock Waiting() {
    GroundBlock out;
    out.Where_ = State::Pending;
    return out;
  }

  [[nodiscard]] const float *Nodes() const noexcept { return Nodes_; }

  [[nodiscard]] TileSpot Spot() const noexcept { return {.Zoom = Zoom_, .X = X_, .Y = Y_}; }

  [[nodiscard]] Sampling Raster() const noexcept { return {.Side = Side_, .Postings = Postings_}; }

private:
  const float *Nodes_ = nullptr;
  long X_ = 0, Y_ = 0;
  int Zoom_ = 0, Side_ = 0;
  uint32_t Postings_ = 0;
  State Where_ = State::Missing;
};

class GroundStream final : public GroundQuery {
public:
  GroundStream(TilePool &tiles, GroundSurface surface);
  ~GroundStream() override;
  GroundStream(const GroundStream &) = delete;
  GroundStream &operator=(const GroundStream &) = delete;

  [[nodiscard]] GroundSample At(LongitudeLatitude at) const override;
  [[nodiscard]] GroundSample Resident(LongitudeLatitude at) const override;
  [[nodiscard]] GroundBlock BlockAt(TileSpot at) const override;

  [[nodiscard]] int BlockZoom() const override { return Surface_.Z; }

  [[nodiscard]] double PostM(double latDeg) const override;

  [[nodiscard]] TilePool &Tiles() { return Tiles_; }

private:
  struct Held;
  friend struct Held;

  [[nodiscard]] const struct Tile *TileAt(long x, long y) const;
  [[nodiscard]] const struct Tile *TileResident(long x, long y) const;
  [[nodiscard]] const struct Tile *CoarseResident(long x, long y) const;
  void KeepCoarse(long x, long y) const;
  [[nodiscard]] GroundSample
  SampleFrom(const struct Tile &tile, int zoom, LongitudeLatitude at) const;

  TilePool &Tiles_;
  GroundSurface Surface_;
  std::unique_ptr<Held> Held_;
};

struct Pooling {
  int Workers = 0;
  double PatienceS = 0.0;
};

[[nodiscard]] TilePool::Config GroundPoolConfig(LongitudeLatitude at, Pooling how = {});

} // namespace outshine::Ground

#endif
