#ifndef OUTSHINE_WORLD_ROADHARVEST_H
#define OUTSHINE_WORLD_ROADHARVEST_H

#include <cstddef>
#include <string>

#include "OsmField.h"
#include "VegetationTemplates.h"
#include "Wayfinding.h"

namespace outshine::World {

struct Reaped {
  size_t Ways = 0;
  size_t Points = 0;
  size_t TooNarrow = 0;
  size_t Unclassed = 0;
  size_t NotALine = 0;
  double NarrowestTakenM = 0.0;
  double WidestRefusedM = 0.0;
  size_t NotACarriageway = 0;
  size_t Ungraded = 0;
  size_t Unlaned = 0;
  std::string WithoutGrade;
  std::string WithoutLanes;
  std::string NotCarriageways;
};

[[nodiscard]] Reaped Reap(const OsmField &field, const VegetationTemplates &widths,
                          double vehicleWidthM, Network &into);

} // namespace outshine::World

#endif
