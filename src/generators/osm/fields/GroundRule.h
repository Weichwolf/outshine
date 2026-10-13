#ifndef OUTSHINE_GENERATORS_OSM_FIELDS_GROUNDRULE_H
#define OUTSHINE_GENERATORS_OSM_FIELDS_GROUNDRULE_H

#include "OsmField.h"
#include "MvtSchema.h"
#include "VegetationTemplates.h"

namespace outshine::Generators::Osm {
[[nodiscard]] inline const Ground::VegetationTemplates::Rule *
GroundRuleOf(const OsmField &field,
             const OsmField::Feature &feature,
             const Ground::VegetationTemplates &templates) {
  const auto layer = field.LayerName(feature.Layer);
  const auto kind = field.Str(feature, "kind");
  const auto *rule = templates.Find(layer, kind);
  return rule == nullptr && field.Schema() == MvtSchema::OpenMapTiles && layer == "sites"
             ? templates.Find("land", kind)
             : rule;
}
}
#endif
