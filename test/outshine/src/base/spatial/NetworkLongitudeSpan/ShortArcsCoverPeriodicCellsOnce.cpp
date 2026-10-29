#include "NetworkLongitudeSpan.h"
#include "Check.h"
#include <algorithm>
#include <cstdint>
#include <initializer_list>
#include <vector>

namespace {
void CheckArc(double from,
              double to,
              int64_t fromColumn,
              int64_t toColumn,
              int64_t columns,
              std::initializer_list<int64_t> expected) {
  using namespace outshine;
  using namespace outshine::Test;
  const auto span = Path::ColumnsAlongShortestArc(
      {.Degrees = from, .Index = fromColumn}, {.Degrees = to, .Index = toColumn}, columns);
  CHECK(span.first <= span.second && span.second - span.first < columns,
        "a periodic interval visits each physical column at most once");
  std::vector<int64_t> actual;
  for (int64_t at = span.first; at <= span.second; ++at) { actual.push_back(at % columns); }
  std::ranges::sort(actual);
  std::vector<int64_t> wanted(expected);
  std::ranges::sort(wanted);
  CHECK(
      actual == wanted,
      "the shortest longitude arc covers every intersected cell including the truncated seam cell");
}
}

int main() {
  using namespace outshine::Test;
  for (const bool reverse : {false, true}) {
    if (reverse) {
      CheckArc(-179, 179, 1, 359, 360, {359, 0, 1});
      CheckArc(4.1, 2.1, 184, 182, 360, {182, 183, 184});
      CheckArc(-170, 100, 0, 0, 2, {0, 1});
      CheckArc(-179, 178, 0, 2, 3, {2, 0});
    } else {
      CheckArc(179, -179, 359, 1, 360, {359, 0, 1});
      CheckArc(2.1, 4.1, 182, 184, 360, {182, 183, 184});
      CheckArc(100, -170, 0, 0, 2, {0, 1});
      CheckArc(178, -179, 2, 0, 3, {2, 0});
    }
  }
  CheckArc(180, -180, 0, 0, 360, {0});
  CheckArc(-180, 180, 0, 0, 360, {0});
  CheckArc(179, -179, 0, 0, 1, {0});
  CheckArc(-179, 179, 0, 0, 1, {0});
  return Report();
}
