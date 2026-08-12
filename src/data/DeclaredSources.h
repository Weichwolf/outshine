#ifndef DECLAREDSOURCES_H
#define DECLAREDSOURCES_H

#include <string>

#include "SourceSet.h"

namespace outshine::Data {

/* WHICH UPSTREAMS EXIST AND AT WHICH RANK, in one place. Three today: the global elevation pyramid,
 * the global vector pyramid, and the star catalogue that has no upstream at all.
 *
 * It is code and not data for exactly as long as it takes the scenario interface to arrive — the
 * registry belongs in the library tier's JSON, where adding an upstream is a declaration and the
 * plugin is only the code that knows how to speak to it. Until then this function is the
 * declaration, and it is the only place in the tree that names all three. */
struct Declared {
  /* Where the star bands are. A source that guessed it could not name what it looked for. */
  std::string StarDirectory;
  /* A run that must not reach a network declares this and gets an Undeclared answer by name instead
   * of a hang. */
  bool WithUpstreams = true;
};

enum class Registered { Complete, RankClash };

[[nodiscard]] Registered RegisterDeclared(SourceSet &set, const Declared &declared);

} // namespace outshine::Data
#endif
