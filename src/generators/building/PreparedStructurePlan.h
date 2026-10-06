#ifndef OUTSHINE_GENERATORS_BUILDING_PREPAREDSTRUCTUREPLAN_H
#define OUTSHINE_GENERATORS_BUILDING_PREPAREDSTRUCTUREPLAN_H

#include "PreparedStructureTile.h"

namespace outshine::Generators {

[[nodiscard]] StructurePlan PreparedStructurePlan(const PreparedStructure &prepared,
                                                  std::span<const double> points,
                                                  std::span<const GeographicRing> holes,
                                                  std::span<const double> corners,
                                                  const Vec3 &anchor);

}
#endif
