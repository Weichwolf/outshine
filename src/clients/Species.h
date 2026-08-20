#ifndef SPECIES_H
#define SPECIES_H

#include "TreeSpecies.h"

namespace outshine::Clients {

[[nodiscard]] bool ReadSpecies(const char *path, Generators::TreeSpecies *out);

}
#endif
