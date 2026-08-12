/* THE GATE, and it must FAIL to build. A
 * generator has no camera, no device and no renderer, and that is enforced by the include set
 * rather than by a rule — so the name below has no spelling here at all. */
#include "Generator.h"
#include "Renderer.h"
// REFUSED: 'Renderer.h' file not found

namespace outshine::Generators {

Render::Renderer *Forbidden();

} // namespace outshine::Generators
