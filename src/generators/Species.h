#ifndef OUTSHINE_GENERATORS_SPECIES_H
#define OUTSHINE_GENERATORS_SPECIES_H

#include <string>
#include <vector>

#include "TreeSpecies.h"

namespace outshine::Generators {

[[nodiscard]] bool ReadSpecies(const char *path, TreeSpecies *out);
[[nodiscard]] bool ReadSpecies(const char *path, std::vector<TreeSpecies> &out, std::string &error);

} // namespace outshine::Generators
#endif
