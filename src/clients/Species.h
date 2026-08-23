#ifndef OUTSHINE_CLIENTS_SPECIES_H
#define OUTSHINE_CLIENTS_SPECIES_H

#include "TreeSpecies.h"

namespace outshine::Clients {

[[nodiscard]] bool ReadSpecies(const char *path, Generators::TreeSpecies *out);

}
#endif
