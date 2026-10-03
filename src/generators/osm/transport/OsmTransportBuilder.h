#ifndef OUTSHINE_GENERATORS_OSM_TRANSPORT_OSMTRANSPORTBUILDER_H
#define OUTSHINE_GENERATORS_OSM_TRANSPORT_OSMTRANSPORTBUILDER_H

#include <cstddef>
#include <cstdint>
#include <expected>
#include <string_view>

#include "OsmElementSet.h"
#include "TransportTopology.h"

namespace outshine::Generators::Osm {

enum class TransportBuildErrorCode : uint8_t {
  MissingSourceObject,
  AmbiguousTag,
  InvalidLayer,
  InvalidOneway,
  InvalidWidth,
  InvalidLaneCount,
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

class TransportBuilder {
public:
  [[nodiscard]] static std::expected<World::TransportTopology, TransportBuildError>
  Build(const ElementSet &source);

  [[nodiscard]] static std::expected<World::TransportTopology, TransportBuildError>
  BuildRegion(const ElementSet &source);

  [[nodiscard]] static std::expected<World::CircuitRoute, CircuitError>
  ResolveCircuit(const World::TransportTopology &graph,
                 const ElementSet &source,
                 uint64_t relationId,
                 std::string_view memberRole = {},
                 size_t maxEdges = 65536);
};

}

#endif
