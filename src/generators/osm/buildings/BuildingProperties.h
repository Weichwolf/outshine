#ifndef OUTSHINE_GENERATORS_OSM_BUILDINGS_BUILDINGPROPERTIES_H
#define OUTSHINE_GENERATORS_OSM_BUILDINGS_BUILDINGPROPERTIES_H

#include "BuildingHeightInterval.h"
#include "OsmField.h"
#include "OsmBuildingFacade.h"
#include "format/CssColour.h"
#include "math/Srgb.h"
#include "math/Vec3.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string_view>

namespace outshine::Generators::Osm {

struct BuildingProperties {
  bool Hidden = false;
  std::optional<Ground::BuildingHeightInterval> Height;
  std::optional<Vec3f> WallColour;
  std::optional<FacadeStyle> Facade;
  bool WallColourRejected = false;
};

[[nodiscard]] inline std::optional<uint32_t> ParseBuildingColour(std::string_view text) noexcept {
  if (text.size() == 6) {
    std::array<char, 7> encoded{'#'};
    std::ranges::copy(text, encoded.begin() + 1);
    if (const auto rgba = ParseCssColour({encoded.data(), encoded.size()})) { return rgba; }
  }
  return ParseCssColour(text);
}

[[nodiscard]] inline BuildingProperties ReadBuildingProperties(const OsmField &field,
                                                               const OsmField::Feature &feature) {
  BuildingProperties result;
  result.Facade = ReadBuildingFacade(field, feature);
  constexpr uint32_t byteMask = 255u;
  if (field.Has(feature, "building:colour")) {
    const auto rgba = ParseBuildingColour(field.Str(feature, "building:colour"));
    result.WallColourRejected = !rgba || (*rgba & byteMask) != byteMask;
    if (!result.WallColourRejected) {
      Vec3f colour{};
      for (size_t channel = 0; channel < 3; ++channel) {
        const float encoded = static_cast<float>((*rgba >> (24 - channel * 8)) & byteMask) / 255.0f;
        colour[channel] = ColourSpace::LinearFromSrgb(encoded);
      }
      result.WallColour = colour;
    }
  }
  if (field.Schema() != MvtSchema::OpenMapTiles) { return result; }
  const bool hidden =
      field.Num(feature, "hide_3d", 0.0) != 0.0 || field.Str(feature, "hide_3d") == "true";
  const double top = field.Num(feature, "height", 0.0);
  const double minimum = field.Num(feature, "min_height", 0.0);
  const auto supplied = field.Integer(feature, "height");
  const bool missing = supplied && !*supplied;
  const bool conflict = !missing && (!std::isfinite(top) || top <= minimum);
  constexpr double defaultBodyHeightM = 5.0;
  result.Hidden = hidden;
  result.Height = Ground::BuildingHeightInterval{
      .TopM = missing || conflict ? minimum + defaultBodyHeightM : top,
      .MinimumM = minimum,
      .TopOrigin = Ground::BuildingHeightOrigin::Generated,
      .MinimumOrigin = Ground::BuildingHeightOrigin::Generated,
      .ConflictingLevels = conflict};
  return result;
}

}
#endif
