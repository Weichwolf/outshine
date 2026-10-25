#ifndef OUTSHINE_ENGINE_TERRAINFIELDPINNING_H
#define OUTSHINE_ENGINE_TERRAINFIELDPINNING_H

#include "HeightField.h"

namespace outshine {
class HeightSheets;

namespace Ground {
class GroundStream;
}

[[nodiscard]] bool PinTerrainField(const HeightSheets *sheets,
                                   const Ground::GroundStream &source,
                                   Data::TileId tile,
                                   Ground::HeightField::Block &into);

[[nodiscard]] bool PinResidentTerrainField(const HeightSheets *sheets,
                                           const Ground::GroundStream &source,
                                           Data::TileId tile,
                                           Ground::HeightField::Block &into);
}
#endif
