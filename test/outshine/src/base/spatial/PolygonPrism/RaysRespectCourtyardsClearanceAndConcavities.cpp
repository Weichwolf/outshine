#include "PolygonPrism.h"
#include "PlanHierarchy.h"
#include "Check.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <optional>
#include <span>

int main() {
  using namespace outshine;
  using namespace outshine::Test;
  constexpr std::array<Vec2, 4> ring{{{{0, 0}}, {{10, 0}}, {{10, 10}}, {{0, 10}}}};
  constexpr std::array<Vec2, 4> courtyard{{{{4, 4}}, {{6, 4}}, {{6, 6}}, {{4, 6}}}};
  const std::array<std::span<const Vec2>, 1> holes{courtyard};
  const PolygonPrism prism{.Ring = ring, .Holes = holes, .Bottom = 2.0, .Top = 12.0};
  const Vec3 left{{-1, 0, 0}};
  const Vec3 up{{0, 0, 1}};
  const Vec3 down{{0, 0, -1}};
  const auto wall = prism.Trace({{-5, 5, 7}}, {{1, 0, 0}}, 0, 100);
  CHECK(wall && wall->Along == 5 && wall->Normal == left,
        "wall is intersected directly from its source edge");
  const auto roof = prism.Trace({{3, 3, 20}}, {{0, 0, -2}}, 0, 100);
  CHECK(roof && roof->Along == 4 && roof->Face == 1 && roof->Normal == up,
        "roof preserves the unnormalized ray parameter");
  CHECK(!prism.Trace({{5, 5, 20}}, {{0, 0, -1}}, 0, 100),
        "a courtyard ray reaches neither roof nor floor");
  CHECK(!prism.Trace({{-5, 5, 1}}, {{1, 0, 0}}, 0, 100),
        "clearance is retained below a raised structure");
  const auto inner = prism.Trace({{5, 5, 7}}, {{1, 0, 0}}, 0, 100);
  CHECK(inner && inner->Along == 1 && inner->Normal == left,
        "courtyard wall faces the empty courtyard for either ring orientation");
  const auto inside = prism.Trace({{3, 5, 7}}, {{0, 0, -1}}, 0, 100);
  CHECK(inside && inside->Along == 5 && inside->Normal == down,
        "a ray originating in the prism can exit its floor");
  auto reversed = ring;
  std::ranges::reverse(reversed);
  const PolygonPrism other{.Ring = reversed, .Holes = holes, .Bottom = 2, .Top = 12};
  const auto back = other.Trace({{-5, 5, 7}}, {{1, 0, 0}}, 0, 100);
  CHECK(back && wall && back->Along == wall->Along && back->Normal == wall->Normal,
        "source ring winding does not change the physical hit");
  constexpr std::array<Vec2, 6> concave{
      {{{0, 0}}, {{10, 0}}, {{10, 4}}, {{4, 4}}, {{4, 10}}, {{0, 10}}}};
  const PolygonPrism elbow{.Ring = concave, .Holes = {}, .Bottom = 0, .Top = 8};
  CHECK(!elbow.Trace({{8, 8, 20}}, {{0, 0, -1}}, 0, 100),
        "bounds do not fill a concavity with roof geometry");
  CHECK(elbow.Trace({{2, 8, 20}}, {{0, 0, -1}}, 0, 100),
        "the adjacent occupied arm retains its roof");
  const std::array bounds{elbow.Bounds(), prism.Bounds()};
  const auto tree = PlanHierarchy::Build(bounds);
  CHECK(tree, "source prisms supply hierarchy bounds without vertices");
  if (tree) {
    const std::array shapes{elbow, prism};
    const auto picked =
        tree->TraceClosest({{-5, 5, 7}},
                           {{1, 0, 0}},
                           0,
                           100,
                           [&](uint32_t source, double maximum) -> std::optional<double> {
                             const auto hit =
                                 shapes[source].Trace({{-5, 5, 7}}, {{1, 0, 0}}, 0, maximum);
                             return hit ? std::optional(hit->Along) : std::nullopt;
                           });
    CHECK(picked && picked->Along == 5, "hierarchy and source intersection compose directly");
  }
  CHECK(!prism.Trace({{-5, 5, 7}}, {{1, 0, 0}}, 0, 4.9), "range excludes a farther wall");
  const std::array<Vec2, 5> repeated{{ring[0], ring[0], ring[1], ring[2], ring[3]}};
  const PolygonPrism duplicate{.Ring = repeated, .Holes = {}, .Bottom = 2, .Top = 12};
  CHECK(!duplicate.Trace({{15, 5, 20}}, {{0, 0, -1}}, 0, 100),
        "a zero-length source edge does not cover the whole roof plane");
  return Report();
}
