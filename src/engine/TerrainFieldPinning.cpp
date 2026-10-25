#include "TerrainFieldPinning.h"
#include "HeightSheets.h"
#include "TerrainLoader.h"

namespace outshine {
bool PinTerrainField(const HeightSheets *sheets,
                     const Ground::GroundStream &source,
                     Data::TileId tile,
                     Ground::HeightField::Block &into) {
  if (sheets != nullptr && sheets->ShareSourcedField(tile, into)) { return true; }
  return Ground::HeightField::SharesField(source.StitchedField(tile), tile, into);
}

bool PinResidentTerrainField(const HeightSheets *sheets,
                             const Ground::GroundStream &source,
                             Data::TileId tile,
                             Ground::HeightField::Block &into) {
  if (sheets != nullptr && sheets->ShareSourcedField(tile, into)) { return true; }
  return Ground::HeightField::SharesField(source.ResidentStitchedField(tile), tile, into);
}
}
