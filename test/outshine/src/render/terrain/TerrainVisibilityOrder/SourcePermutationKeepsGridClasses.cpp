#include "TerrainVisibilityOrder.h"
#include "Check.h"
#include <array>
#include <cmath>
#include <limits>

int main() {
  using namespace outshine::Render;
  using namespace outshine::Test;
  const double nan = std::numeric_limits<double>::quiet_NaN();
  const double infinity = std::numeric_limits<double>::infinity();
  std::array<TerrainVisibleSource, 7> sources{
      {{2, nan}, {6, 1}, {4, -infinity}, {0, 20}, {5, 1}, {1, 5}, {3, -9}}};
  OrderTerrainSources(sources, 3);
  constexpr std::array<size_t, 7> expected{1, 0, 2, 3, 5, 6, 4};
  for (size_t i = 0; i < sources.size(); ++i) {
    CHECK(sources[i].Index == expected[i],
          "source identity follows depth inside the real and virtual grid classes");
  }
  CHECK(std::isinf(sources[2].Depth) && sources[2].Depth > 0 && std::isinf(sources[6].Depth) &&
            sources[6].Depth > 0,
        "unknown positive and negative depth states stay present at a deterministic fallback");
  CHECK(sources[4].Depth == 1 && sources[5].Depth == 1,
        "equal finite depths retain their exact value and source-index tie order");
  OrderTerrainSources(sources, 3);
  for (size_t i = 0; i < sources.size(); ++i) {
    CHECK(sources[i].Index == expected[i], "the same input scope produces the same order");
  }
  OrderTerrainSources({}, 0);
  std::array<TerrainVisibleSource, 1> only{{{42, nan}}};
  OrderTerrainSources(only, 0);
  CHECK(only[0].Index == 42 && std::isinf(only[0].Depth),
        "an unknown single virtual source survives unchanged in identity");
  return Report();
}
