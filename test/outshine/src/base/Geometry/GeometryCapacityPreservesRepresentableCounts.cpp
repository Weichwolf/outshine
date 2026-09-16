#include "GeometryCapacity.h"
#include "Check.h"
#include <array>
#include <limits>

int main() {
  using namespace outshine;
  using namespace outshine::Test;
  constexpr size_t indexLimit = static_cast<size_t>(std::numeric_limits<int>::max());
  constexpr size_t maximum = std::numeric_limits<size_t>::max();
  static_assert(CanAppendGeometryEntry(indexLimit - 1, maximum));
  static_assert(!CanAppendGeometryEntry(indexLimit, maximum));
  CHECK(!CanAppendGeometryEntry(0, 0), "zero-capacity storage refuses its first entry");
  CHECK(CanAppendGeometryEntry(0, 1), "one slot accepts exactly one entry");
  CHECK(!CanAppendGeometryEntry(1, 1), "full storage refuses before growth");
  for (size_t count : std::array<size_t, 3>{indexLimit, indexLimit + 1, maximum}) {
    CHECK(!CanAppendGeometryEntry(count, maximum), "count cannot exceed public int range");
  }
  CHECK(CanAppendGeometryEntry(6, 7) && !CanAppendGeometryEntry(7, 7),
        "a smaller container limit dominates index representability");
  CHECK(!CanAppendGeometryEntry(maximum, 7), "invalid large counts cannot wrap into capacity");
  return Report();
}
