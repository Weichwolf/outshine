/* THE SPECIES DECLARATION, READ. A declaration is a file on disk in one toolchain and a preloaded
 * image in the other, so the read belongs to the client; the parse belongs to the declaration. */
#ifndef SPECIES_H
#define SPECIES_H

#include "TreeSpecies.h"

namespace outshine::Clients {

[[nodiscard]] bool ReadSpecies(const char *path, Generators::TreeSpecies *out);

} // namespace outshine::Clients
#endif
