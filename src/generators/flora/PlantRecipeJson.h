#ifndef OUTSHINE_GENERATORS_FLORA_PLANTRECIPEJSON_H
#define OUTSHINE_GENERATORS_FLORA_PLANTRECIPEJSON_H

#include "PlantRecipe.h"
#include <cstddef>
#include <expected>
#include <string_view>

namespace outshine::Generators {
[[nodiscard]] std::expected<PlantRecipe, std::string_view> ReadPlantRecipe(std::string_view text,
                                                                           size_t maximumBytes);
}
#endif
