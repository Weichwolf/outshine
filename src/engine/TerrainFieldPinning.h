#ifndef OUTSHINE_ENGINE_TERRAINFIELDPINNING_H
#define OUTSHINE_ENGINE_TERRAINFIELDPINNING_H

#include "HeightField.h"

namespace outshine {
namespace Generators {
class TerrainInputPreparation;
}

namespace Ground {
class GroundStream;
}

[[nodiscard]] bool PinTerrainField(const Generators::TerrainInputPreparation *inputs,
                                   const Ground::GroundStream &source,
                                   Data::TileId tile,
                                   Ground::HeightField::Block &into);

}
#endif
