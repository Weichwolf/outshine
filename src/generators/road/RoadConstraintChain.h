#ifndef OUTSHINE_GENERATORS_ROAD_ROADCONSTRAINTCHAIN_H
#define OUTSHINE_GENERATORS_ROAD_ROADCONSTRAINTCHAIN_H

#include <cstddef>
#include <cstdint>
#include <expected>
#include <span>
#include <vector>

#include "Earth.h"
#include "HeightField.h"
#include "TransportTopology.h"

namespace outshine::Generators {

struct RoadConstraintPoint {
  uint64_t SourceNodeId = 0;
  LongitudeLatitude Geographic;
  double TerrainElevationM = 0.0;
  EastNorthUp TerrainLocalM;
};

struct RoadConstraintEdge {
  World::TransportEdgeId SourceEdge;
  uint32_t FromPointIndex = 0;
  uint32_t ToPointIndex = 0;
  World::TransportFacility Facility = World::TransportFacility::Unknown;
  World::TransportSurface Surface = World::TransportSurface::Unknown;
  double WidthM = 0.0;
  int32_t Layer = 0;
  bool Bridge = false;
  bool Tunnel = false;
};

enum class RoadConstraintErrorCode : uint8_t {
  SourceMismatch,
  UnqualifiedTerrain,
  EmptySelection,
  TooManyEdges,
  DuplicateEdge,
  MissingEdge,
  UnusableEdge,
  DisconnectedEdge,
  MissingNode,
  MissingTerrain,
  DegenerateGeometry
};

struct RoadConstraintError {
  RoadConstraintErrorCode Code = RoadConstraintErrorCode::EmptySelection;
  World::TransportEdgeId Edge;
  uint64_t SourceNodeId = 0;
};

class RoadConstraintChain {
public:
  [[nodiscard]] static std::expected<RoadConstraintChain, RoadConstraintError>
  Build(const World::TransportTopology &topology,
        const Data::OsmSourceIdentity &selectionSource,
        std::span<const World::TransportEdgeId> selectedEdges,
        const Ground::HeightField &terrain);

  [[nodiscard]] const Data::OsmSourceIdentity &SourceIdentity() const noexcept {
    return SourceIdentity_;
  }

  [[nodiscard]] uint64_t TerrainDigest() const noexcept { return TerrainDigest_; }

  [[nodiscard]] std::span<const Data::TileSourceIdentity> TerrainSources() const noexcept {
    return TerrainSources_;
  }

  [[nodiscard]] LongitudeLatitude Anchor() const noexcept { return Anchor_; }

  [[nodiscard]] std::span<const RoadConstraintPoint> Points() const noexcept { return Points_; }

  [[nodiscard]] std::span<const RoadConstraintEdge> Edges() const noexcept { return Edges_; }

  [[nodiscard]] bool Closed() const noexcept { return Closed_; }

  [[nodiscard]] double EstimatedLengthM() const noexcept { return EstimatedLengthM_; }

private:
  Data::OsmSourceIdentity SourceIdentity_;
  uint64_t TerrainDigest_ = 0;
  std::vector<Data::TileSourceIdentity> TerrainSources_;
  LongitudeLatitude Anchor_;
  std::vector<RoadConstraintPoint> Points_;
  std::vector<RoadConstraintEdge> Edges_;
  bool Closed_ = false;
  double EstimatedLengthM_ = 0.0;
};

}

#endif
