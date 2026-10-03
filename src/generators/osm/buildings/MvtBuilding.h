#ifndef OUTSHINE_GENERATORS_OSM_BUILDINGS_MVTBUILDING_H
#define OUTSHINE_GENERATORS_OSM_BUILDINGS_MVTBUILDING_H

#include "BuildingHeightInterval.h"
#include "OsmField.h"
#include <cmath>
#include <optional>

namespace outshine::Generators::Osm {

struct MvtBuilding {
  bool Hidden = false;
  std::optional<Ground::BuildingHeightInterval> Height;
};

[[nodiscard]] inline MvtBuilding ReadMvtBuilding(const OsmField &field,
                                                 const OsmField::Feature &feature) {
  if (field.Schema() != MvtSchema::OpenMapTiles) { return {}; }
  const bool hidden =
      field.Num(feature, "hide_3d", 0.0) != 0.0 || field.Str(feature, "hide_3d") == "true";
  const double top = field.Num(feature, "height", 0.0);
  const double minimum = field.Num(feature, "min_height", 0.0);
  const auto supplied = field.Integer(feature, "height");
  const bool missing = supplied && !*supplied;
  const bool conflict = !missing && (!std::isfinite(top) || top <= minimum);
  constexpr double defaultBodyHeightM = 5.0;
  return {.Hidden = hidden,
          .Height = Ground::BuildingHeightInterval{
              .TopM = missing || conflict ? minimum + defaultBodyHeightM : top,
              .MinimumM = minimum,
              .TopOrigin = Ground::BuildingHeightOrigin::Generated,
              .MinimumOrigin = Ground::BuildingHeightOrigin::Generated,
              .ConflictingLevels = conflict}};
}

}
#endif
