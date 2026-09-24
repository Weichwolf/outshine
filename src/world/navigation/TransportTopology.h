#ifndef OUTSHINE_WORLD_NAVIGATION_TRANSPORTTOPOLOGY_H
#define OUTSHINE_WORLD_NAVIGATION_TRANSPORTTOPOLOGY_H

#include <compare>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "OsmElements.h"

namespace outshine::World {

enum class TransportMode : uint8_t { Motor = 1, Walk = 2, Cycle = 4, Rail = 8, Water = 16 };
enum class EdgeDirection : uint8_t { Forward, Reverse };
enum class TransportAccess : uint8_t { Public, Restricted, Forbidden };

struct TransportEdgeId {
  uint64_t WayId = 0;
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

enum class TransportBuildErrorCode : uint8_t {
  MissingSourceObject,
  AmbiguousTag,
  InvalidLayer,
  InvalidOneway,
  DegenerateSegment,
  TooManyEdges
};

struct TransportBuildError {
  TransportBuildErrorCode Code = TransportBuildErrorCode::MissingSourceObject;
  uint64_t SourceId = 0;
};

enum class CircuitErrorCode : uint8_t {
  SourceMismatch,
  MissingRelation,
  NotCircuit,
  InvalidMember,
  MissingWay,
  MissingEdge,
  UnusableEdge,
  EmptyRoute,
  AmbiguousDirection,
  DisconnectedRoute,
  TooManyEdges
};

struct CircuitError {
  CircuitErrorCode Code = CircuitErrorCode::MissingRelation;
  uint64_t SourceId = 0;
};

struct CircuitRoute {
  Data::OsmSourceIdentity SourceIdentity;
  uint64_t RelationId = 0;
  uint64_t StartNodeId = 0;
  std::vector<TransportEdgeId> EdgeIds;
};

class TransportTopology {
public:
  [[nodiscard]] static std::expected<TransportTopology, TransportBuildError>
  Build(const Data::OsmElements &source);

  [[nodiscard]] const Data::OsmSourceIdentity &SourceIdentity() const noexcept {
    return SourceIdentity_;
  }

  [[nodiscard]] std::span<const TransportNode> Nodes() const noexcept { return Nodes_; }

  [[nodiscard]] std::span<const TransportEdge> Edges() const noexcept { return Edges_; }

  [[nodiscard]] size_t UnclassifiedWayCount() const noexcept { return UnclassifiedWayCount_; }

  [[nodiscard]] const TransportNode *FindNode(uint64_t id) const noexcept;
  [[nodiscard]] const TransportEdge *FindEdge(TransportEdgeId id) const noexcept;
  [[nodiscard]] std::span<const OutgoingTransportEdge> OutgoingFrom(uint64_t nodeId) const noexcept;
  [[nodiscard]] static bool CanContinue(const TransportEdge &from,
                                        const TransportEdge &to) noexcept;

  [[nodiscard]] std::expected<CircuitRoute, CircuitError>
  ResolveCircuit(const Data::OsmElements &source,
                 uint64_t relationId,
                 std::string_view memberRole = {},
                 size_t maxEdges = 65536) const;

private:
  Data::OsmSourceIdentity SourceIdentity_;
  std::vector<TransportNode> Nodes_;
  std::vector<TransportEdge> Edges_;
  std::vector<OutgoingTransportEdge> Outgoing_;
  size_t UnclassifiedWayCount_ = 0;
};

}

#endif
