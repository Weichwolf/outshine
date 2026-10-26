#ifndef OUTSHINE_GENERATORS_TRANSPORT_TRANSPORTCORESELECTION_H
#define OUTSHINE_GENERATORS_TRANSPORT_TRANSPORTCORESELECTION_H

#include "Earth.h"
#include "GeoCellId.h"
#include "TransportBoundary.h"
#include <array>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <memory>
#include <optional>
#include <stop_token>
#include <vector>

namespace outshine::World {
class TransportNetworkSnapshot;
}

namespace outshine::Generators {
struct TransportCoreEnd {
  double Fraction = 0;
  LongitudeLatitude Geographic{};
  std::array<std::optional<TransportBoundary>, 2> Boundaries{};
};

struct TransportCoreSegment {
  uint32_t SourceEdge = 0;
  std::array<TransportCoreEnd, 2> Ends{};
};

struct TransportCoreWork {
  size_t ValidatedNodes = 0;
  size_t ValidatedEdges = 0;
  size_t SourceEdgesVisited = 0;
  size_t PhysicalSegmentsVisited = 0;
  size_t NodeLookups = 0;
  size_t WindowTests = 0;
  size_t BoundingRejections = 0;
  size_t ClipDivisions = 0;
  size_t ContainerCapacityBytes = 0;
};

struct TransportCoreSelection {
  GeoCellId Core{};
  std::shared_ptr<const World::TransportNetworkSnapshot> Source;
  std::vector<TransportCoreSegment> Segments;
  TransportCoreWork Work{};
};

struct TransportCoreLimits {
  size_t SourceNodesMost = 0;
  size_t SourceEdgesMost = 0;
  size_t SegmentsMost = 0;
  size_t ContainerCapacityMost = 0;
};

enum class TransportCoreError : uint8_t {
  InvalidCore,
  MissingSource,
  InvalidSource,
  AmbiguousSegment,
  CapacityExceeded,
  Canceled
};

[[nodiscard]] std::expected<TransportCoreSelection, TransportCoreError>
SelectTransportCore(GeoCellId core,
                    std::shared_ptr<const World::TransportNetworkSnapshot> source,
                    TransportCoreLimits limits,
                    const std::stop_token &stop = {});
}
#endif
