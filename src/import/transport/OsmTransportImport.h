#ifndef OUTSHINE_IMPORT_TRANSPORT_OSMTRANSPORTIMPORT_H
#define OUTSHINE_IMPORT_TRANSPORT_OSMTRANSPORTIMPORT_H

#include <cstddef>
#include <cstdint>
#include <expected>
#include <string_view>

#include "OsmElements.h"
#include "TransportTopology.h"

namespace outshine::Import {

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

class OsmTransportImport {
public:
  [[nodiscard]] static std::expected<World::TransportTopology, TransportBuildError>
  Build(const Data::OsmElements &source);

  [[nodiscard]] static std::expected<World::TransportTopology, TransportBuildError>
  BuildRegion(const Data::OsmElements &source);

  [[nodiscard]] static std::expected<World::CircuitRoute, CircuitError>
  ResolveCircuit(const World::TransportTopology &graph,
                 const Data::OsmElements &source,
                 uint64_t relationId,
                 std::string_view memberRole = {},
                 size_t maxEdges = 65536);
};

}

#endif
