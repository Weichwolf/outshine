#include "PlanHierarchy.h"
#include "Check.h"

#include <cmath>
#include <limits>
#include <optional>
#include <vector>

int main() {
  using namespace outshine;
  using namespace outshine::Test;
  std::vector<Box> bounds;
  for (size_t at = 0; at < 128; ++at) {
    const double x = 10.0 + static_cast<double>(127 - at) * 8.0;
    bounds.push_back({.Min = {{x - 1, -1, -1}}, .Max = {{x + 1, 1, 1}}});
  }
  const auto tree = PlanHierarchy::Build(bounds);
  CHECK(tree, "source hierarchy builds without meshes");
  if (!tree) { return Report(); }
  size_t calls = 0;
  const auto sphere = [&](uint32_t source, double maximum) -> std::optional<double> {
    ++calls;
    const double centre = bounds[source].Middle()[0];
    const double distance = centre - 1.0;
    return distance <= maximum ? std::optional(distance) : std::nullopt;
  };
  const Vec3 origin{}, forward{{1, 0, 0}};
  const double unlimited = std::numeric_limits<double>::infinity();
  const auto hit = tree->TraceClosest(origin, forward, 0.0, unlimited, sphere);
  CHECK(hit && hit->Source == 127 && hit->Along == 9.0,
        "nearest source wins despite reverse source order");
  CHECK(calls == 1, "a front hit rejects every farther overlapping branch before source work");
  calls = 0;
  CHECK(!tree->TraceClosest({{0, 2, 0}}, forward, 0.0, unlimited, sphere),
        "a parallel ray outside the slab hits nothing");
  CHECK(calls == 0, "root rejection performs no source intersection");
  CHECK(!tree->TraceClosest(origin, {{-1, 0, 0}}, 0.0, unlimited, sphere),
        "sources behind the ray are excluded");
  CHECK(!tree->TraceClosest(origin, forward, 0.0, 8.0, sphere),
        "finite range excludes the first source before intersection");
  CHECK(!tree->TraceClosest(origin, {}, 0.0, unlimited, sphere), "stationary ray is invalid");
  CHECK(!tree->TraceClosest(origin, forward, -1.0, unlimited, sphere),
        "negative minimum is invalid");
  CHECK(!tree->TraceClosest(origin, forward, 0.0, std::nan(""), sphere), "NaN range is invalid");
  calls = 0;
  const auto ignored =
      tree->TraceClosest(origin,
                         forward,
                         0.0,
                         unlimited,
                         [&](uint32_t source, double maximum) -> std::optional<double> {
                           if (source == 127) { return std::nan(""); }
                           return sphere(source, maximum);
                         });
  CHECK(ignored && ignored->Source == 126 && ignored->Along == 17.0,
        "invalid narrow-phase hit does not hide the next source");
  const auto diagonal =
      tree->TraceClosest({{0, 10, 0}},
                         {{1, -1, 0}},
                         0.0,
                         unlimited,
                         [](uint32_t source, double) -> std::optional<double> {
                           return source == 127 ? std::optional(10.0) : std::nullopt;
                         });
  CHECK(diagonal && diagonal->Along == 10.0, "ray parameters need no normalization");
  return Report();
}
