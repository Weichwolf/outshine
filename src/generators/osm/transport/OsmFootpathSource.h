#ifndef OUTSHINE_GENERATORS_OSM_TRANSPORT_OSMFOOTPATHSOURCE_H
#define OUTSHINE_GENERATORS_OSM_TRANSPORT_OSMFOOTPATHSOURCE_H

#include "OsmSurfacePlacement.h"
#include <array>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <optional>
#include <string_view>

namespace outshine::Generators::Osm {
class OsmField;
class StreetField;

struct FootpathSourceFacts {
  SurfacePlacement Placement;
  std::string_view Kind;
  double HalfWidthM = 0;
  std::optional<bool> FootAccess;
  std::array<bool, 2> OwnsEnds{};
};

enum class FootpathSourceError : uint8_t { InvalidSource, InvalidPlacement, InvalidAccess };

[[nodiscard]] std::expected<FootpathSourceFacts, FootpathSourceError>
ReadFootpathSource(const OsmField &field, const StreetField &ways, size_t way);
}
#endif
