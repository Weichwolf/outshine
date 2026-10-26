#ifndef OUTSHINE_GENERATORS_CONTRACTS_TRANSPORTBOUNDARY_H
#define OUTSHINE_GENERATORS_CONTRACTS_TRANSPORTBOUNDARY_H

#include "GeoCellId.h"
#include <cstdint>
#include <expected>

namespace outshine::Generators {
enum class TransportCoreSide : uint8_t { West, East, South, North };
enum class TransportBoundaryAxis : uint8_t { Longitude, Latitude };
enum class TransportBoundaryError : uint8_t { InvalidCore, InvalidSide };

struct TransportBoundary {
  int Level = 0;
  TransportBoundaryAxis Axis = TransportBoundaryAxis::Longitude;
  uint32_t Line = 0;
  uint32_t Segment = 0;

  [[nodiscard]] bool Valid() const noexcept;
  [[nodiscard]] bool operator==(const TransportBoundary &) const noexcept = default;
};

[[nodiscard]] std::expected<TransportBoundary, TransportBoundaryError>
TransportBoundaryOf(GeoCellId core, TransportCoreSide side) noexcept;
}
#endif
