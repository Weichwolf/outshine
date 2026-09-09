#ifndef OUTSHINE_GENERATORS_BUILDING_BUILDINGCUT_H
#define OUTSHINE_GENERATORS_BUILDING_BUILDINGCUT_H

#include <span>

namespace outshine::Generators {

[[nodiscard]] constexpr bool HasSingleCut(std::span<const int> sides) noexcept {
  int first = 0;
  int previous = 0;
  int crossings = 0;
  for (const int side : sides) {
    if (side == 0) { continue; }
    if (side != -1 && side != 1) { return false; }
    if (first == 0) {
      first = side;
    } else if (side != previous) {
      ++crossings;
    }
    if (crossings > 2) { return false; }
    previous = side;
  }
  if (previous != first) { ++crossings; }
  return crossings == 2;
}

}
#endif
