#ifndef OUTSHINE_WORLD_GROUND_TERRAINLOADER_H
#define OUTSHINE_WORLD_GROUND_TERRAINLOADER_H
#include <stdint.h>

#include <memory>
#include <optional>
#include <expected>
#include <string_view>
#include <string>
#include <unordered_map>
#include <vector>

#include "ChunkSurface.h"
#include "GroundQuery.h"
#include "GroundSample.h"
#include "TerrainCertificate.h"
#include <cstddef>
#include "TileSourceIdentity.h"
#include "TilePool.h"
#include "TerrainSamplingCoverage.h"

namespace outshine::Data {
class SourceSet;
class Transport;
}

namespace outshine::Ground {

constexpr int kStreamGrid = 64;

struct GroundSurface {
  int Z;
  int Grid;
};

class TerrainField;
class TerrainGrid;
class TerrainTiles;

[[nodiscard]] double
TileHeightAslM(const float *nodes, int side, uint32_t postings, double fx, double fy);

class GroundBlock {
public:
  enum class State { Resolved, Pending, Missing };

  [[nodiscard]] State Where() const noexcept { return Where_; }

  void AslMRow(LongitudeLatitude from, double lonStepDeg, std::span<double> out) const noexcept;

  static GroundBlock Over(const float *nodes,
                          TileSpot at,
                          Sampling raster,
                          std::span<const Data::TileSourceIdentity> sources = {},
                          bool missingBoundary = false,
                          const TerrainCertificate *certificate = nullptr) {
    GroundBlock out;
    out.Nodes_ = nodes;
    out.Zoom_ = at.Zoom;
    out.X_ = at.X;
    out.Y_ = at.Y;
    out.Side_ = raster.Side;
    out.Postings_ = raster.Postings;
    out.Sources_ = sources;
    out.MissingBoundary_ = missingBoundary;
    out.Certificate_ = certificate;
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

  [[nodiscard]] std::span<const Data::TileSourceIdentity> Sources() const noexcept {
    return Sources_;
  }

  [[nodiscard]] const TerrainCertificate *Certificate() const noexcept { return Certificate_; }

  [[nodiscard]] bool HasMissingBoundary() const noexcept { return MissingBoundary_; }

private:
  const float *Nodes_ = nullptr;
  long X_ = 0, Y_ = 0;
  int Zoom_ = 0, Side_ = 0;
  uint32_t Postings_ = 0;
  std::span<const Data::TileSourceIdentity> Sources_;
  bool MissingBoundary_ = false;
  const TerrainCertificate *Certificate_ = nullptr;
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

  [[nodiscard]] TerrainGrid FieldOf(Data::TileId of) const;
  [[nodiscard]] std::expected<TerrainRevisionIndex::Reservation, TerrainRevisionIndex::Error>
  PrepareSourceMetadata(std::span<const Data::TileId> fields) const;

  [[nodiscard]] std::shared_ptr<const TerrainField> StitchedField(Data::TileId of) const;
  [[nodiscard]] std::shared_ptr<const TerrainField> ResidentStitchedField(Data::TileId of) const;
  [[nodiscard]] TilePool::Reply
  PollStitchedField(Data::TileId of,
                    std::shared_ptr<const TerrainField> &out,
                    std::optional<Data::FetchFailure> *failure = nullptr) const;

  [[nodiscard]] size_t HeapBytes() const;

  [[nodiscard]] int BlockZoom() const override { return Surface_.Z; }

  [[nodiscard]] double PostM(double latDeg) const override;

  [[nodiscard]] std::optional<TerrainSamplingCoverage>
  SamplingCoverage(Data::TileId source) const noexcept;
  [[nodiscard]] std::optional<TerrainSamplingCoverage>
  SamplingCoverage(LongitudeLatitude at) const noexcept;

  [[nodiscard]] TilePool &Tiles() { return Tiles_; }

  [[nodiscard]] uint64_t TerrainScopeRevision() const noexcept {
    return Tiles_.TerrainScopeRevision();
  }

private:
  struct Held;
  friend struct Held;

  void SynchronizeTerrainScope() const;

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

[[nodiscard]] std::expected<TilePool::Config, std::string_view>
GroundPoolConfig(LongitudeLatitude at, Pooling how = {});

}

#endif
