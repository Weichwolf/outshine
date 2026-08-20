/* THE GATE, and it must FAIL to build. A generator produces a result, never a line: Log and the
 * telemetry bus live in core/io, which is not on this include set, so scattered output has no
 * spelling here. */
#include "Generator.h"
#include "Log.h"
// REFUSED: 'Log.h' file not found

namespace outshine::Generators {

void Forbidden() { Log::Info("generator", "spoke", {}); }

} // namespace outshine::Generators
