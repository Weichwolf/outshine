/* THE VECTOR SOURCE'S OWN LAYER NAMES, in one place. Every one of them is a word the tile provider
 * chose (shortbread schema), so it is a fact about the source and not a taxonomy of ours — which is
 * exactly why it must be spelled once: a layer the provider renames is then one edit, and a layer
 * this engine asks for and nobody declares in the class table cannot be spelled at all.
 *
 * The class table (src/assets/world/vegetation.json) names the SAME words as data, because it maps
 * (layer, kind) to a template. That is not a second place for this statement: the table decides what
 * a layer MEANS, this decides which layers are fetched. */
#ifndef OSMLAYER_H
#define OSMLAYER_H

#include <initializer_list>
#include <string>
#include <vector>

namespace outshine::World {

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
  for (OsmLayer layer : layers) names.emplace_back(OsmLayerName(layer));
  return names;
}

} // namespace outshine::World
#endif
