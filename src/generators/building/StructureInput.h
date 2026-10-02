#ifndef OUTSHINE_GENERATORS_BUILDING_STRUCTUREINPUT_H
#define OUTSHINE_GENERATORS_BUILDING_STRUCTUREINPUT_H

#include "StructureBake.h"
#include "StructureFootprints.h"
#include <expected>

namespace outshine::Generators {

enum class StructureInputError : uint8_t { InvalidCell };

[[nodiscard]] std::expected<RawTile, StructureInputError>
StructureInput(outshine::Ground::StructureFootprints footprints);

}

#endif
