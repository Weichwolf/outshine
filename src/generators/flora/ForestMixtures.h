#ifndef OUTSHINE_GENERATORS_FLORA_FORESTMIXTURES_H
#define OUTSHINE_GENERATORS_FLORA_FORESTMIXTURES_H

#include "SpeciesMixture.h"
#include <expected>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace outshine::Generators {
class RegionSurfaceRecipes;
}

namespace outshine::Generators {

class TreeSpecies;

[[nodiscard]] std::expected<std::vector<SpeciesMixture>, std::string>
ReadForestMixtures(std::string_view path,
                   const RegionSurfaceRecipes &templates,
                   std::span<const TreeSpecies> species);

}
#endif
