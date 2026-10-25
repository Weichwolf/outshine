#include "TerrainFieldPinning.h"
#include "TerrainInputPreparation.h"
#include "TerrainLoader.h"

namespace outshine {
bool PinTerrainField(const Generators::TerrainInputPreparation *inputs,
                     const Ground::GroundStream &source,
                     Data::TileId tile,
                     Ground::HeightField::Block &into) {
  if (inputs != nullptr && inputs->ShareSourcedField(tile, into)) { return true; }
  return Ground::HeightField::SharesField(source.StitchedField(tile), tile, into);
}
}
