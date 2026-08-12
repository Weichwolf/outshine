/* THE POSITIVE HALF OF THE GATE. A generator translation unit compiles against core and nothing
 * else, and this proves the set is usable — without it the two negative halves below would pass for
 * the wrong reason the day the compile line breaks. */
#include "ClusterDag.h"
#include "Geodesy.h"
#include "Json.h"
#include "Units.h"

namespace outshine::Generators {

double MetresPerDegree() { return kMPerDeg; }

} // namespace outshine::Generators
