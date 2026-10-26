#include "TransportBoundary.h"
#include <cstdint>
#include <expected>

namespace outshine::Generators {
bool TransportBoundary::Valid() const noexcept {
  if (Level < 0 || Level > GeoCellId::MaximumLevel) { return false; }
  const uint32_t count = uint32_t{1} << static_cast<unsigned>(Level);
  if (Segment >= count) { return false; }
  switch (Axis) {
    case TransportBoundaryAxis::Longitude: return Line < count;
    case TransportBoundaryAxis::Latitude: return Line <= count;
  }
  return false;
}

std::expected<TransportBoundary, TransportBoundaryError>
TransportBoundaryOf(GeoCellId core, TransportCoreSide side) noexcept {
  if (!core.Valid()) { return std::unexpected(TransportBoundaryError::InvalidCore); }
  const uint32_t count = uint32_t{1} << static_cast<unsigned>(core.Level);
  switch (side) {
    case TransportCoreSide::West:
      return TransportBoundary{.Level = core.Level,
                               .Axis = TransportBoundaryAxis::Longitude,
                               .Line = core.X,
                               .Segment = core.Y};
    case TransportCoreSide::East:
      return TransportBoundary{.Level = core.Level,
                               .Axis = TransportBoundaryAxis::Longitude,
                               .Line = (core.X + 1) % count,
                               .Segment = core.Y};
    case TransportCoreSide::South:
      return TransportBoundary{.Level = core.Level,
                               .Axis = TransportBoundaryAxis::Latitude,
                               .Line = core.Y,
                               .Segment = core.X};
    case TransportCoreSide::North:
      return TransportBoundary{.Level = core.Level,
                               .Axis = TransportBoundaryAxis::Latitude,
                               .Line = core.Y + 1,
                               .Segment = core.X};
  }
  return std::unexpected(TransportBoundaryError::InvalidSide);
}
}
