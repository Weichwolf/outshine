/* THE GATE, and it must FAIL to build: `make verify-generators` asserts the compile error. A
 * generator has no camera, no device and no renderer, and that is enforced by the include set
 * rather than by a rule — so the name below has no spelling here at all. */
#include "Renderer.h"

namespace outshine::Generators {

Render::Renderer *Forbidden();

} // namespace outshine::Generators
