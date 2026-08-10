/* THE GATE, and it must FAIL to build. `generators/` never includes `world/` either: a generator
 * reads a ground view handed to it, never the streamer that produced it. */
#include "World.h"

namespace outshine::Generators {

World::World *Forbidden();

} // namespace outshine::Generators
