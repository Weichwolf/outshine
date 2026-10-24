#ifndef OUTSHINE_GENERATORS_OSM_TRANSPORT_OSMSURFACEPLACEMENT_H
#define OUTSHINE_GENERATORS_OSM_TRANSPORT_OSMSURFACEPLACEMENT_H

#include <cstdint>
#include <expected>
#include <optional>

namespace outshine::Generators::Osm {
class OsmField;

struct SurfacePlacement {
  std::optional<int32_t> Layer = std::nullopt;
  std::optional<bool> Bridge = std::nullopt;
  std::optional<bool> Tunnel = std::nullopt;
};

enum class SurfacePlacementError : uint8_t {
  InvalidLayer,
  InvalidFeature,
  InvalidBridge,
  InvalidTunnel,
  ConflictingStructure
};

[[nodiscard]] std::expected<SurfacePlacement, SurfacePlacementError>
ReadSurfacePlacement(const OsmField &field, uint32_t feature);
}
#endif
