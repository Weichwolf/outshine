#ifndef OUTSHINE_WORLD_GROUND_OSMLAYER_H
#define OUTSHINE_WORLD_GROUND_OSMLAYER_H

#include <initializer_list>
#include <string>
#include <vector>

namespace outshine::Ground {

enum class OsmLayer { Buildings, WaterPolygons, WaterLines, Streets, StreetPolygons };

inline const char *OsmLayerName(OsmLayer layer) {
  switch (layer) {
    case OsmLayer::Buildings: return "buildings";
    case OsmLayer::WaterPolygons: return "water_polygons";
    case OsmLayer::WaterLines: return "water_lines";
    case OsmLayer::Streets: return "streets";
    case OsmLayer::StreetPolygons: return "street_polygons";
  }
  return "";
}

inline std::vector<std::string> OsmLayerNames(std::initializer_list<OsmLayer> layers) {
  std::vector<std::string> names;
  names.reserve(layers.size());
  for (OsmLayer layer : layers) { names.emplace_back(OsmLayerName(layer)); }
  return names;
}

}
#endif
