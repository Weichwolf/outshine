#ifndef OUTSHINE_ENGINE_STREAMING_ROADTERRAINPINJOB_H
#define OUTSHINE_ENGINE_STREAMING_ROADTERRAINPINJOB_H

#include <cstddef>
#include <cstdint>
#include <expected>
#include <memory>
#include <optional>
#include <span>
#include <vector>

#include "SourcedTerrainFields.h"
#include "TransportTopology.h"

namespace outshine {

enum class RoadTerrainPinErrorCode : uint8_t {
  SourceMismatch,
  InvalidZoom,
  EmptyRoute,
  TooManyEdges,
  MissingEdge,
  MissingNode,
  TooManyTiles
};

struct RoadTerrainPinError {
  RoadTerrainPinErrorCode Code = RoadTerrainPinErrorCode::EmptyRoute;
  World::TransportEdgeId Edge;
  uint64_t SourceNodeId = 0;
};

struct PinnedRoadTerrain {
  uint64_t CandidateGeneration = 0;
  Data::OsmSourceIdentity SourceIdentity;
  std::shared_ptr<const Ground::HeightField> Heights;

  [[nodiscard]] bool Matches(uint64_t generation,
                             const Data::OsmSourceIdentity &source) const noexcept {
    return CandidateGeneration == generation && SourceIdentity == source;
  }
};

struct RoadTerrainPinRequest {
  int Zoom = 0;
  size_t MaximumTiles = 0;
  uint64_t CandidateGeneration = 0;
};

struct RoadTerrainTileSelectionRequest {
  int Zoom = 0;
  size_t MaximumTiles = 0;
};

class RoadTerrainPinJob {
public:
  [[nodiscard]] static std::expected<std::vector<Data::TileId>, RoadTerrainPinError>
  SelectTiles(const World::TransportTopology &topology,
              const Data::OsmSourceIdentity &selectionSource,
              std::span<const World::TransportEdgeId> route,
              RoadTerrainTileSelectionRequest request);

  [[nodiscard]] static std::expected<RoadTerrainPinJob, RoadTerrainPinError>
  Begin(SourcedTerrainFields fields,
        const World::TransportTopology &topology,
        const Data::OsmSourceIdentity &selectionSource,
        std::span<const World::TransportEdgeId> route,
        RoadTerrainPinRequest request);

  [[nodiscard]] bool Advance(size_t tilesMost);

  [[nodiscard]] bool Complete() const noexcept { return Result_.has_value(); }

  [[nodiscard]] size_t TileCount() const noexcept { return Tiles_.size(); }

  [[nodiscard]] size_t CopiedTiles() const noexcept { return NextTile_; }

  [[nodiscard]] std::optional<Data::TileId> PendingTile() const noexcept;

  [[nodiscard]] const std::optional<PinnedRoadTerrain> &Result() const noexcept { return Result_; }

private:
  SourcedTerrainFields Fields_;
  Data::OsmSourceIdentity SourceIdentity_;
  std::vector<Data::TileId> Tiles_;
  std::vector<Ground::HeightField::Block> Blocks_;
  std::optional<PinnedRoadTerrain> Result_;
  size_t NextTile_ = 0;
  uint64_t CandidateGeneration_ = 0;
  int Zoom_ = 0;
};

}

#endif
