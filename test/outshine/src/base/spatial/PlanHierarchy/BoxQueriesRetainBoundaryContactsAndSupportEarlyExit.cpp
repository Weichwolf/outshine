#include "PlanHierarchy.h"
#include "Check.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <limits>
#include <vector>

int main() {
  using namespace outshine;
  using namespace outshine::Test;
  const std::array bounds{Box{.Min = {{0, 0, 0}}, .Max = {{1, 1, 0}}},
                          Box{.Min = {{1, 0, 0}}, .Max = {{2, 1, 0}}},
                          Box{.Min = {{4, 4, 1}}, .Max = {{5, 5, 2}}}};
  const auto tree = PlanHierarchy::Build(bounds);
  CHECK(tree.has_value(), "planar and spatial bounds share the existing hierarchy");
  if (!tree) { return Report(); }
  std::vector<uint32_t> hits;
  const auto collect = [&](uint32_t source) {
    hits.push_back(source);
    return true;
  };
  CHECK(tree->Query(Box{.Min = {{1, .5, 0}}, .Max = {{1, .5, 0}}}, collect),
        "a closed degenerate query finishes normally");
  std::ranges::sort(hits);
  CHECK((hits == std::vector<uint32_t>{0, 1}), "both owners of an exact boundary are retained");
  hits.clear();
  CHECK(tree->Query(Box{.Min = {{-1, -1, -1}}, .Max = {{6, 6, 3}}}, collect),
        "the complete enclosing query finishes");
  CHECK(hits.size() == bounds.size(), "all distinct source leaves are emitted once");
  size_t calls = 0;
  CHECK(!tree->Query(Box{.Min = {{-1, -1, -1}}, .Max = {{6, 6, 3}}},
                     [&](uint32_t) {
                       ++calls;
                       return false;
                     }),
        "a consumer can stop before allocating a full candidate list");
  CHECK(calls == 1, "early exit invokes no later consumer");
  hits.clear();
  Box invalid{.Min = {{0, 5, 0}}, .Max = {{10, 4, 1}}};
  CHECK(tree->Query(invalid, collect) && hits.empty(), "an empty secondary axis emits nothing");
  invalid.Min[1] = std::numeric_limits<double>::quiet_NaN();
  CHECK(tree->Query(invalid, collect) && hits.empty(),
        "a nonfinite unordered query cannot select every leaf");
  return Report();
}
