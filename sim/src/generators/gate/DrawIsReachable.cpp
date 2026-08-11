/* THE PICTURE HALF OF THE LAYER, and it compiles only where a renderer exists to hand instances to.
 * Built with the client include set; the negative half beside it asserts that the server's does not
 * reach it. */
#include "DrawSet.h"
#include "DrawSink.h"
#include "DrawSource.h"

namespace outshine::Generators {

size_t Sources(const DrawSet &set) { return set.Count(); }

} // namespace outshine::Generators
