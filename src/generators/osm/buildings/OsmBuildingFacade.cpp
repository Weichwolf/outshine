#include "OsmBuildingFacade.h"
#include <array>
#include <optional>
#include <span>
#include <string_view>
#include <utility>

namespace outshine::Generators::Osm {
std::optional<FacadeStyle> BuildingFacadeOf(std::string_view kind) {
  using enum FacadeStyle;
  static constexpr std::array rules{std::pair{"house", House},
                                    std::pair{"detached", House},
                                    std::pair{"semidetached_house", House},
                                    std::pair{"terrace", Terrace},
                                    std::pair{"apartments", Block},
                                    std::pair{"residential", Block},
                                    std::pair{"office", Block},
                                    std::pair{"commercial", Block},
                                    std::pair{"retail", Block},
                                    std::pair{"hotel", Block},
                                    std::pair{"school", Block},
                                    std::pair{"university", Block},
                                    std::pair{"hospital", Block},
                                    std::pair{"civic", Block},
                                    std::pair{"industrial", Hall},
                                    std::pair{"warehouse", Hall},
                                    std::pair{"storage_tank", Tower},
                                    std::pair{"silo", Tower},
                                    std::pair{"chimney", Tower},
                                    std::pair{"water_tower", Tower},
                                    std::pair{"tower", Tower},
                                    std::pair{"shed", Outbuilding},
                                    std::pair{"garage", Outbuilding},
                                    std::pair{"garages", Outbuilding},
                                    std::pair{"roof", Outbuilding},
                                    std::pair{"greenhouse", Glazing}};
  for (const auto &[name, style] : rules) {
    if (kind == name) { return style; }
  }
  return std::nullopt;
}

std::optional<FacadeStyle> ReadBuildingFacade(const OsmField &field,
                                              const OsmField::Feature &feature) {
  for (const auto *const key :
       {"man_made", "building:part", "building", "subclass", "class", "kind"}) {
    if (auto style = BuildingFacadeOf(field.Str(feature, key))) { return style; }
  }
  return std::nullopt;
}

std::optional<FacadeStyle> ReadBuildingFacade(std::span<const Tag> tags) {
  for (const auto *const key :
       {"man_made", "building:part", "building", "subclass", "class", "kind"}) {
    for (const Tag &tag : tags) {
      if (tag.Key != key) { continue; }
      if (auto style = BuildingFacadeOf(tag.Value)) { return style; }
    }
  }
  return std::nullopt;
}
}
