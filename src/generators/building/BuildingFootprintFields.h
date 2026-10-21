#ifndef OUTSHINE_GENERATORS_BUILDING_BUILDINGFOOTPRINTFIELDS_H
#define OUTSHINE_GENERATORS_BUILDING_BUILDINGFOOTPRINTFIELDS_H

#include "BuildingFootprint.h"
#include <type_traits>
#include <utility>

namespace outshine::Generators::StructureBinary {

template <typename Archive, typename Ceiling> bool TerrainParts(Archive &archive, Ceiling &value) {
  const auto point = [](auto &held, auto &item) {
    return held.Number(item.EastM) && held.Number(item.NorthM);
  };
  const auto part = [point](auto &held, auto &item) {
    return held.Number(item.HeightAslM) && held.List(item.Ring, point) &&
           held.List(item.Holes,
                     [point](auto &next, auto &hole) { return next.List(hole, point); });
  };
  if constexpr (std::is_const_v<Ceiling>) {
    const std::vector<Ground::BuildingTerrainPart> empty;
    return archive.List(value.Parts ? *value.Parts : empty, part);
  } else {
    std::vector<Ground::BuildingTerrainPart> parts;
    if (!archive.List(parts, part)) { return false; }
    if (!parts.empty()) {
      if (archive.AllocationLeft < sizeof(std::vector<Ground::BuildingTerrainPart>)) {
        return false;
      }
      archive.AllocationLeft -= sizeof(std::vector<Ground::BuildingTerrainPart>);
      value.Parts =
          std::make_shared<const std::vector<Ground::BuildingTerrainPart>>(std::move(parts));
    }
    return true;
  }
}

template <typename Archive, typename Footprint>
bool FootprintFields(Archive &archive,
                     Footprint &value,
                     bool terrainFields,
                     bool partFields = true) {
  auto &street = value.Street;
  return archive.Number(value.FirstPoint) && archive.Number(value.PointCount) &&
         archive.Number(value.FirstHole) && archive.Number(value.HoleCount) &&
         archive.Number(value.HeightM) && archive.Number(value.MinimumHeightM) &&
         archive.Number(value.BaseM) && archive.Number(value.SeatM) &&
         archive.Number(value.FootM) && archive.Number(value.Source) &&
         archive.Number(street.Known) && archive.Number(street.KerbEm) &&
         archive.Number(street.KerbNm) && archive.Number(street.AlongE) &&
         archive.Number(street.AlongN) && archive.Number(street.ToStreetE) &&
         archive.Number(street.ToStreetN) &&
         (!terrainFields ||
          archive.Maybe(value.TerrainCeiling, [partFields, &value](auto &held, auto &ceiling) {
            return held.Number(ceiling.OriginPoint) && held.Number(ceiling.HeightAslM) &&
                   (!partFields || TerrainParts(held, value.TerrainContacts));
          }));
}

}
#endif
