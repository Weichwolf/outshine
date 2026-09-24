#ifndef OUTSHINE_GENERATORS_ROAD_ROADALIGNMENT_H
#define OUTSHINE_GENERATORS_ROAD_ROADALIGNMENT_H

#include <cstddef>
#include <cstdint>
#include <expected>
#include <optional>
#include <span>
#include <vector>

#include "Earth.h"
#include "RoadConstraintChain.h"
#include "math/Vec3.h"

namespace outshine::Generators {

struct RoadAlignmentEdge {
  World::TransportEdgeId SourceEdge;
  double StartStationM = 0.0;
  double EndStationM = 0.0;
  double SourceWidthM = 0.0;
  double StartWidthM = 0.0;
  double EndWidthM = 0.0;
  World::TransportSurface Surface = World::TransportSurface::Unknown;
};

struct RoadAlignmentPose {
  World::TransportEdgeId SourceEdge;
  double StationM = 0.0;
  double EdgeStationM = 0.0;
  EastNorthUp PositionM;
  Vec3 TangentEnu;
  double WidthM = 0.0;
  double BankRad = 0.0;
  World::TransportSurface Surface = World::TransportSurface::Unknown;
};

enum class RoadAlignmentErrorCode : uint8_t {
  InvalidConstraints,
  StructureNeedsSolver,
  SharpTurn,
  SampleBudgetExceeded,
  InvalidArcLength
};

struct RoadAlignmentError {
  RoadAlignmentErrorCode Code = RoadAlignmentErrorCode::InvalidConstraints;
  World::TransportEdgeId SourceEdge;
  uint64_t SourceNodeId = 0;
};

class RoadAlignmentBuilder;

class RoadAlignment {
public:
  RoadAlignment(RoadAlignment &&) noexcept = default;
  RoadAlignment &operator=(RoadAlignment &&) noexcept = default;
  RoadAlignment(const RoadAlignment &) = delete;
  RoadAlignment &operator=(const RoadAlignment &) = delete;

  [[nodiscard]] const Data::OsmSourceIdentity &SourceIdentity() const noexcept {
    return SourceIdentity_;
  }

  [[nodiscard]] uint64_t TerrainDigest() const noexcept { return TerrainDigest_; }

  [[nodiscard]] std::span<const Data::TileSourceIdentity> TerrainSources() const noexcept {
    return TerrainSources_;
  }

  [[nodiscard]] LongitudeLatitude Anchor() const noexcept { return Anchor_; }

  [[nodiscard]] bool Closed() const noexcept { return Closed_; }

  [[nodiscard]] double LengthM() const noexcept { return LengthM_; }

  [[nodiscard]] std::span<const RoadAlignmentEdge> Edges() const noexcept { return Edges_; }

  [[nodiscard]] size_t OwnedHeapBytes() const noexcept {
    return TerrainSources_.capacity() * sizeof(Data::TileSourceIdentity) +
           Edges_.capacity() * sizeof(RoadAlignmentEdge) + Curves_.capacity() * sizeof(Cubic) +
           ArcRanges_.capacity() * sizeof(ArcRange) + ArcSamples_.capacity() * sizeof(ArcSample) +
           EdgeIndicesById_.capacity() * sizeof(uint32_t);
  }

  [[nodiscard]] const RoadAlignmentEdge *FindEdge(World::TransportEdgeId id) const noexcept;
  [[nodiscard]] std::optional<RoadAlignmentPose> AtEdgeStation(World::TransportEdgeId id,
                                                               double edgeStationM) const noexcept;
  [[nodiscard]] std::optional<RoadAlignmentPose> AtStation(double stationM) const noexcept;

private:
  friend class RoadAlignmentBuilder;

  struct Cubic {
    Vec3 StartM;
    Vec3 EndM;
    Vec3 StartDerivativeM;
    Vec3 EndDerivativeM;
  };

  struct ArcSample {
    double Parameter = 0.0;
    double DistanceM = 0.0;
  };

  struct ArcRange {
    uint32_t Begin = 0;
    uint32_t Count = 0;
  };

  struct EdgeStation {
    size_t EdgeIndex = 0;
    double DistanceM = 0.0;
  };

  RoadAlignment() = default;
  [[nodiscard]] static Vec3 Evaluate(const Cubic &curve, double parameter) noexcept;
  [[nodiscard]] static Vec3 Derivative(const Cubic &curve, double parameter) noexcept;
  [[nodiscard]] std::optional<RoadAlignmentPose> SampleEdge(EdgeStation request) const noexcept;
  [[nodiscard]] std::optional<size_t> FindEdgeIndex(World::TransportEdgeId id) const noexcept;

  Data::OsmSourceIdentity SourceIdentity_;
  uint64_t TerrainDigest_ = 0;
  std::vector<Data::TileSourceIdentity> TerrainSources_;
  LongitudeLatitude Anchor_;
  bool Closed_ = false;
  double LengthM_ = 0.0;
  std::vector<RoadAlignmentEdge> Edges_;
  std::vector<Cubic> Curves_;
  std::vector<ArcRange> ArcRanges_;
  std::vector<ArcSample> ArcSamples_;
  std::vector<uint32_t> EdgeIndicesById_;
};

class RoadAlignmentBuilder {
public:
  [[nodiscard]] static std::expected<RoadAlignment, RoadAlignmentError>
  Build(const RoadConstraintChain &constraints);

private:
  [[nodiscard]] static std::expected<void, RoadAlignmentError>
  AppendEdge(RoadAlignment &alignment,
             const RoadConstraintChain &constraints,
             std::span<const Vec3> points,
             std::span<const Vec3> tangents,
             std::span<const double> nodeWidths,
             size_t index);
};

}

#endif
