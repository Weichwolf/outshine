#ifndef OUTSHINE_GENERATORS_TRANSPORT_TRANSPORTCORECLIPPING_H
#define OUTSHINE_GENERATORS_TRANSPORT_TRANSPORTCORECLIPPING_H

#include "TransportCoreSelection.h"
#include "TransportTopology.h"
#include <array>
#include <cstddef>
#include <cstdint>
#include <expected>

namespace outshine::Generators {
struct TransportCoreWindow {
  GeographicBounds Bounds{};
  std::array<TransportBoundary, 4> Boundaries{};
};

struct ClippedTransportSegments {
  std::array<TransportCoreSegment, 2> Segments;
  size_t Count = 0;
};

[[nodiscard]] std::expected<TransportCoreWindow, TransportCoreError>
TransportWindowOf(GeoCellId core) noexcept;

[[nodiscard]] std::expected<ClippedTransportSegments, TransportCoreError>
ClipTransportCore(const TransportCoreWindow &window,
                  uint32_t edge,
                  const World::TransportNode &from,
                  const World::TransportNode &to,
                  TransportCoreWork &work) noexcept;
}
#endif
