/* THE GATE, and it must FAIL to build. Reading the height without asking where it came from is the
 * defect that cost the buildings a round and then reappeared in the forest — so it is a compile
 * error, not a review finding. */
#include "GroundSample.h"

namespace outshine {

double Forbidden(const GroundSample &g) { return g.AslM; }

} // namespace outshine
