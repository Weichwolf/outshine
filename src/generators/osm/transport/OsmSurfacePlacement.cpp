#include "OsmSurfacePlacement.h"
#include "OsmField.h"
#include <cmath>
#include <cstdint>
#include <expected>
#include <limits>
#include <optional>
#include <string_view>

namespace outshine::Generators::Osm {
namespace {
std::expected<std::optional<bool>, SurfacePlacementError> Flag(const OsmField &field,
                                                               const OsmField::Feature &feature,
                                                               const char *key,
                                                               SurfacePlacementError error) {
  if (!field.Has(feature, key)) { return std::nullopt; }
  const double number = field.Num(feature, key, std::numeric_limits<double>::quiet_NaN());
  if (std::isfinite(number)) {
    if (number == 0 || number == 1) { return number == 1; }
    return std::unexpected(error);
  }
  const std::string_view text = field.Str(feature, key);
  if (text == "yes" || text == "true" || text == "1") { return true; }
  if (text == "no" || text == "false" || text == "0") { return false; }
  return std::unexpected(error);
}
}

std::expected<SurfacePlacement, SurfacePlacementError> ReadSurfacePlacement(const OsmField &field,
                                                                            uint32_t featureIndex) {
  if (featureIndex >= field.Features().size()) {
    return std::unexpected(SurfacePlacementError::InvalidFeature);
  }
  const auto &feature = field.Features()[featureIndex];
  const auto layer = field.Integer(feature, "layer");
  if (!layer) { return std::unexpected(SurfacePlacementError::InvalidLayer); }
  const auto bridge = Flag(field, feature, "bridge", SurfacePlacementError::InvalidBridge);
  if (!bridge) { return std::unexpected(bridge.error()); }
  const auto tunnel = Flag(field, feature, "tunnel", SurfacePlacementError::InvalidTunnel);
  if (!tunnel) { return std::unexpected(tunnel.error()); }
  if (*bridge == true && *tunnel == true) {
    return std::unexpected(SurfacePlacementError::ConflictingStructure);
  }
  return SurfacePlacement{.Layer = *layer, .Bridge = *bridge, .Tunnel = *tunnel};
}
}
