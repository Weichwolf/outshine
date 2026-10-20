#ifndef OUTSHINE_GENERATORS_BUILDING_BUILDINGFOOTPRINTFIELDS_H
#define OUTSHINE_GENERATORS_BUILDING_BUILDINGFOOTPRINTFIELDS_H

#include "BuildingFootprint.h"

namespace outshine::Generators::StructureBinary {

template <typename Archive, typename Footprint>
bool FootprintFields(Archive &archive, Footprint &value, bool terrainFields) {
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
         (!terrainFields || archive.Maybe(value.TerrainCeiling, [](auto &held, auto &ceiling) {
           return held.Number(ceiling.OriginPoint) && held.Number(ceiling.HeightAslM);
         }));
}

}
#endif
