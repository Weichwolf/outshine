#ifndef OUTSHINE_GENERATORS_OSM_PROVIDERS_MVTSCHEMA_H
#define OUTSHINE_GENERATORS_OSM_PROVIDERS_MVTSCHEMA_H

#include "MvtLayer.h"
#include <optional>
#include <string_view>

namespace outshine::Generators::Osm {

enum class MvtSchema : uint8_t { Shortbread, OpenMapTiles };

[[nodiscard]] inline std::optional<MvtSchema> ParseMvtSchema(std::string_view name) noexcept {
  if (name.empty() || name == "shortbread") { return MvtSchema::Shortbread; }
  if (name == "openmaptiles") { return MvtSchema::OpenMapTiles; }
  return std::nullopt;
}

[[nodiscard]] inline std::string_view MvtLayerName(MvtSchema schema,
                                                   std::string_view canonical) noexcept {
  if (schema == MvtSchema::Shortbread) { return canonical; }
  if (canonical == "buildings") { return "building"; }
  if (canonical == "water_polygons") { return "water"; }
  if (canonical == "water_lines") { return "waterway"; }
  if (canonical == "streets") { return "transportation"; }
  if (canonical == "street_polygons") { return {}; }
  return canonical;
}

[[nodiscard]] inline MvtLayer::Tag
NormalizeMvtTag(MvtSchema schema, std::string_view canonical, MvtLayer::Tag tag) noexcept {
  if (schema == MvtSchema::Shortbread) { return tag; }
  if (canonical == "buildings") {
    if (tag.Key == "render_height") { tag.Key = "height"; }
    if (tag.Key == "render_min_height") { tag.Key = "min_height"; }
  }
  if (tag.Key == "class") {
    tag.Key = "kind";
    if (canonical == "streets" && !tag.IsNumber) {
      if (tag.String == "minor") { tag.String = "residential"; }
      if (tag.String == "rail") { tag.String = "railway"; }
    }
  }
  if (canonical == "streets" && tag.Key == "brunnel" && !tag.IsNumber &&
      (tag.String == "bridge" || tag.String == "tunnel")) {
    tag.Key = tag.String;
    tag.String = {};
    tag.Number = 1.0;
    tag.IsNumber = true;
  }
  return tag;
}

}
#endif
