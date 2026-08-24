#ifndef OUTSHINE_CLIENTS_SPECIES_H
#define OUTSHINE_CLIENTS_SPECIES_H

#include <string>
#include <vector>

#include "TreeSpecies.h"

namespace outshine::Clients {

[[nodiscard]] bool ReadSpecies(const char *path, Generators::TreeSpecies *out);
[[nodiscard]] bool ReadSpecies(const char *path, std::vector<Generators::TreeSpecies> &out,
                               std::string &error);

}
#endif
