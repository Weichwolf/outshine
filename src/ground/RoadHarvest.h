#ifndef OUTSHINE_GROUND_ROADHARVEST_H
#define OUTSHINE_GROUND_ROADHARVEST_H

#include <cstddef>
#include <string_view>
#include <string>

#include "OsmField.h"
#include "VegetationTemplates.h"
#include "Wayfinding.h"

namespace outshine::Ground {

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
  std::string WithoutGrade;
  std::string NotCarriageways;
  bool StreetsAbsent = false;
};

// the diagnostic lists are space-terminated tokens: "way" is not yet listed just because
// "motorway " is -- a substring match under-reports the classes the list exists to name
[[nodiscard]] inline bool Listed(const std::string &list, std::string_view kind) {
  for (size_t at = list.find(kind); at != std::string::npos; at = list.find(kind, at + 1)) {
    const bool starts = at == 0 || list[at - 1] == ' ';
    const bool ends = at + kind.size() < list.size() && list[at + kind.size()] == ' ';
    if (starts && ends) { return true; }
  }
  return false;
}

[[nodiscard]] Reaped Reap(const OsmField &field, const VegetationTemplates &widths,
                          double vehicleWidthM, Path::Network &into);

} // namespace outshine::Ground

#endif
