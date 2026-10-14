#include "HipRoofPlan.h"

#include <cstddef>
#include <iostream>
#include <print>
#include <vector>

int main() {
  using namespace outshine;
  using namespace outshine::Generators;
  size_t ringCount = 0;
  if (!(std::cin >> ringCount) || ringCount == 0 || ringCount > 32) { return 2; }
  std::vector<std::vector<EastNorth>> rings(ringCount);
  size_t vertices = 0;
  for (auto &ring : rings) {
    size_t count = 0;
    if (!(std::cin >> count) || count < 3 || count > 256 - vertices) { return 2; }
    vertices += count;
    ring.resize(count);
    for (auto &point : ring) {
      if (!(std::cin >> point.EastM >> point.NorthM)) { return 2; }
    }
  }
  const auto plan = HipRoofPlan::Prepare(rings.front(), std::span(rings).subspan(1), {});
  if (!plan) {
    std::println("error {}", static_cast<int>(plan.error()));
    return 1;
  }
  std::println("plan {:.17g} {:.17g} {:.17g} {:.17g} {}",
               plan->Origin().EastM,
               plan->Origin().NorthM,
               plan->MaximumDistanceM(),
               plan->PrecisionM(),
               plan->Facets().size());
  for (const auto &facet : plan->Facets()) {
    std::println("facet {} {} {:.17g} {:.17g} {:.17g} {}",
                 facet.SourceRing,
                 facet.SourceEdge,
                 facet.Gradient.EastM,
                 facet.Gradient.NorthM,
                 facet.InterceptM,
                 facet.Ring.size());
    for (const auto point : facet.Ring) {
      std::println("{:.17g} {:.17g}", point.EastM, point.NorthM);
    }
  }
  size_t queries = 0;
  if (!(std::cin >> queries) || queries > 32768) { return 2; }
  for (size_t at = 0; at < queries; ++at) {
    EastNorth point;
    if (!(std::cin >> point.EastM >> point.NorthM)) { return 2; }
    if (const auto distance = plan->DistanceAt(point)) {
      std::println("height {:.17g}", *distance);
    } else {
      std::println("outside");
    }
  }
  return 0;
}
