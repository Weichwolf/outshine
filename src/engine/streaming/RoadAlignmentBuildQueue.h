#ifndef OUTSHINE_ENGINE_STREAMING_ROADALIGNMENTBUILDQUEUE_H
#define OUTSHINE_ENGINE_STREAMING_ROADALIGNMENTBUILDQUEUE_H

#include <cstddef>
#include <cstdint>
#include <expected>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "OsmTransportLoader.h"
#include "RoadAlignment.h"
#include "SourcedTerrainFields.h"
#include "Tasks.h"

namespace outshine {

struct NamedRoadAlignment {
  std::string Id;
  std::shared_ptr<const Generators::RoadAlignment> Alignment;
};

enum class RoadAlignmentBuildErrorCode : uint8_t {
  InvalidRequest,
  TerrainPin,
  MissingTerrain,
  Constraints,
  Alignment
};

struct RoadAlignmentBuildError {
  RoadAlignmentBuildErrorCode Code = RoadAlignmentBuildErrorCode::InvalidRequest;
  std::string RouteId;
  std::optional<Data::TileId> Tile;
  World::TransportEdgeId Edge;
};

struct RoadAlignmentBuildProduct {
  uint64_t CandidateGeneration = 0;
  Data::OsmSourceIdentity SourceIdentity;
  std::vector<NamedRoadAlignment> Routes;

  [[nodiscard]] bool Matches(uint64_t generation,
                             const Data::OsmSourceIdentity &source) const noexcept {
    return CandidateGeneration == generation && SourceIdentity == source;
  }
};

struct RoadAlignmentBuildRequest {
  std::shared_ptr<const World::TransportNetworkSnapshot> Source;
  SourcedTerrainFields Terrain;
  std::vector<size_t> RouteIndices;
  int TerrainZoom = 0;
  uint64_t CandidateGeneration = 0;
};

class RoadAlignmentBuildQueue {
public:
  using Result = std::expected<RoadAlignmentBuildProduct, RoadAlignmentBuildError>;

  [[nodiscard]] bool TryStart(Tasks &tasks, RoadAlignmentBuildRequest request);
  void Poll(Tasks &tasks, uint64_t activeCandidateGeneration);
  [[nodiscard]] std::optional<Result> TakeCompleted();

  [[nodiscard]] bool Busy() const noexcept { return Pending_.has_value(); }

private:
  struct Pending {
    Tasks::Handle Handle = Tasks::kNoTask;
    uint64_t CandidateGeneration = 0;
    std::shared_ptr<std::optional<Result>> Output;
  };

  [[nodiscard]] static Result Build(RoadAlignmentBuildRequest request);

  std::optional<Pending> Pending_;
  std::optional<Result> Completed_;
  uint64_t CompletedGeneration_ = 0;
};

}

#endif
