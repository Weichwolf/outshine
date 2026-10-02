#ifndef OUTSHINE_WORLD_NAVIGATION_TRANSPORTTOPOLOGY_H
#define OUTSHINE_WORLD_NAVIGATION_TRANSPORTTOPOLOGY_H

#include <compare>
#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

#include "SourceIdentity.h"

namespace outshine::World {

enum class TransportMode : uint8_t { Motor = 1, Walk = 2, Cycle = 4, Rail = 8, Water = 16 };
enum class EdgeDirection : uint8_t { Forward, Reverse };
enum class TransportAccess : uint8_t { Public, Restricted, Forbidden };
enum class TransportFacility : uint8_t {
  Unknown,
  Motorway,
  Trunk,
  Arterial,
  LocalStreet,
  ServiceRoad,
  Track,
  Raceway,
  Walkway,
  Cycleway,
  Railway,
  Waterway,
  Ferry,
  Mixed
};
enum class TransportSurface : uint8_t {
  Unknown,
  Asphalt,
  Concrete,
  Paved,
  Gravel,
  Earth,
  Rail,
  Water
};

struct TransportEdgeId {
  uint64_t PathId = 0;
  uint32_t SegmentOrdinal = 0;
  EdgeDirection Direction = EdgeDirection::Forward;

  [[nodiscard]] auto operator<=>(const TransportEdgeId &) const noexcept = default;
};

struct TransportNode {
  uint64_t SourceNodeId = 0;
  double LatitudeDeg = 0.0;
  double LongitudeDeg = 0.0;
};

struct TransportEdge {
  TransportEdgeId Id;
  uint64_t FromNodeId = 0;
  uint64_t ToNodeId = 0;
  uint8_t Modes = 0;
  TransportFacility Facility = TransportFacility::Unknown;
  TransportSurface Surface = TransportSurface::Unknown;
  double WidthM = 0.0;
  uint8_t LaneCount = 0;
  int32_t Layer = 0;
  bool Bridge = false;
  bool Tunnel = false;
  TransportAccess Access = TransportAccess::Public;

  [[nodiscard]] bool Allows(TransportMode mode) const noexcept {
    return (Modes & static_cast<uint8_t>(mode)) != 0 && Access != TransportAccess::Forbidden;
  }
};

struct OutgoingTransportEdge {
  uint64_t NodeId = 0;
  uint32_t EdgeIndex = 0;
};

struct CircuitRoute {
  Data::SourceIdentity SourceIdentity;
  uint64_t SourceRouteId = 0;
  uint64_t StartNodeId = 0;
  std::vector<TransportEdgeId> EdgeIds;
};

class TransportTopology {
public:
  TransportTopology() = default;
  TransportTopology(Data::SourceIdentity identity,
                    std::vector<TransportNode> nodes,
                    std::vector<TransportEdge> edges,
                    size_t unclassifiedPathCount = 0);

  [[nodiscard]] const Data::SourceIdentity &SourceIdentity() const noexcept {
    return SourceIdentity_;
  }

  [[nodiscard]] std::span<const TransportNode> Nodes() const noexcept { return Nodes_; }

  [[nodiscard]] std::span<const TransportEdge> Edges() const noexcept { return Edges_; }

  [[nodiscard]] size_t UnclassifiedPathCount() const noexcept { return UnclassifiedPathCount_; }

  [[nodiscard]] const TransportNode *FindNode(uint64_t id) const noexcept;
  [[nodiscard]] const TransportEdge *FindEdge(TransportEdgeId id) const noexcept;
  [[nodiscard]] std::span<const OutgoingTransportEdge> OutgoingFrom(uint64_t nodeId) const noexcept;
  [[nodiscard]] static bool CanContinue(const TransportEdge &from,
                                        const TransportEdge &to) noexcept;

private:
  Data::SourceIdentity SourceIdentity_;
  std::vector<TransportNode> Nodes_;
  std::vector<TransportEdge> Edges_;
  std::vector<OutgoingTransportEdge> Outgoing_;
  size_t UnclassifiedPathCount_ = 0;
};

}

#endif
