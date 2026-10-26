#include "Check.h"
#include "SurfaceRibbon.h"
#include <array>
#include <cmath>
#include <limits>
#include <stop_token>
#include <utility>
#include <vector>

namespace {
using namespace outshine;
using namespace outshine::Generators;
constexpr double kScalarTolerance = 1e-12;

void Straight() {
  const std::array<PlanarPoint, 4> axis{{{-5, 0}, {0, 0}, {5, 0}, {15, 0}}};
  const std::array<PlanarPoint, 5> first{{{0, -2}, {0, -1}, {0, 0}, {0, 1}, {0, 2}}};
  const std::array<PlanarPoint, 3> last{{{10, -4}, {10, 0}, {10, 4}}};
  const SurfaceRibbonEnd start{.StationM = 5, .Profile = first, .AxisVertex = 2};
  const SurfaceRibbonEnd end{.StationM = 15, .Profile = last, .AxisVertex = 1};
  const auto ribbon = BuildSurfaceRibbon(axis, start, end);
  CHECK(ribbon.has_value(), "complete transverse profiles construct a cropped ribbon");
  if (!ribbon) { return; }
  const std::vector<PlanarPoint> expectedAxis{{0, 0}, {5, 0}, {10, 0}};
  const std::vector<PlanarPoint> expectedRing{
      {0, -2}, {0, -1}, {0, 0}, {0, 1}, {0, 2}, {5, 3}, {10, 4}, {10, 0}, {10, -4}, {5, -3}};
  CHECK(ribbon->Axis == expectedAxis, "crop retains original interior source knots");
  CHECK(ribbon->Domain.Rings.size() == 1 && ribbon->Domain.Rings[0] == expectedRing,
        "all profile vertices remain unchanged, including transverse interior knots");
  CHECK(std::abs(ribbon->AreaM2 - 60) < kScalarTolerance,
        "variable endpoint widths bound a trapezoid with analytic area 10*(4+8)/2");
  CHECK(ribbon->MaximumMiter == 1, "a straight ribbon introduces no width amplification");
  const auto crowded = BuildSurfaceRibbon(axis, start, end, {.RingPoints = 9});
  CHECK(!crowded && crowded.error() == SurfaceRibbonError::CapacityExceeded,
        "ring capacity rejects the complete proposal instead of dropping profile knots");
  const auto reversed = BuildSurfaceRibbon(axis, end, start);
  CHECK(!reversed && reversed.error() == SurfaceRibbonError::InvalidInput,
        "reversed source station order cannot change profile ownership");
  auto invalid = start;
  invalid.AxisVertex = 0;
  const auto edge = BuildSurfaceRibbon(axis, invalid, end);
  CHECK(!edge && edge.error() == SurfaceRibbonError::InvalidProfile,
        "an outside profile edge cannot masquerade as its original source axis");
  auto unordered = first;
  std::swap(unordered[0], unordered[1]);
  invalid.Profile = unordered;
  invalid.AxisVertex = 2;
  const auto crossing = BuildSurfaceRibbon(axis, invalid, end);
  CHECK(!crossing && crossing.error() == SurfaceRibbonError::InvalidProfile,
        "transverse profile reversals cannot publish a folded boundary");
}

void Bend() {
  const std::array<PlanarPoint, 3> axis{{{0, 0}, {5, 0}, {5, 5}}};
  const std::array<PlanarPoint, 3> first{{{0, -1}, {0, 0}, {0, 1}}};
  const std::array<PlanarPoint, 3> last{{{6, 5}, {5, 5}, {4, 5}}};
  const SurfaceRibbonEnd start{.StationM = 0, .Profile = first, .AxisVertex = 1};
  const SurfaceRibbonEnd end{.StationM = 10, .Profile = last, .AxisVertex = 1};
  const auto ribbon = BuildSurfaceRibbon(axis, start, end);
  CHECK(ribbon && std::abs(ribbon->AreaM2 - 20) < kScalarTolerance,
        "a right angle maintains two metre width and analytic area 2*(5+5)");
  CHECK(ribbon && std::abs(ribbon->MaximumMiter - std::sqrt(2.0)) < kScalarTolerance,
        "right-angle miter length follows the exact sqrt(2) construction");
  auto invalid = last;
  invalid[2][1] += 1;
  const auto skew =
      BuildSurfaceRibbon(axis, start, {.StationM = 10, .Profile = invalid, .AxisVertex = 1});
  CHECK(!skew && skew.error() == SurfaceRibbonError::InvalidProfile,
        "a nontransverse outside profile is refused instead of flattened");
  auto duplicate = axis;
  duplicate[1] = duplicate[0];
  const auto zero = BuildSurfaceRibbon(duplicate, start, end);
  CHECK(!zero && zero.error() == SurfaceRibbonError::InvalidInput,
        "duplicate source vertices cannot create undefined tangents");
  auto nonfinite = axis;
  nonfinite[1][0] = std::numeric_limits<double>::quiet_NaN();
  const auto nan = BuildSurfaceRibbon(nonfinite, start, end);
  CHECK(!nan && nan.error() == SurfaceRibbonError::InvalidInput,
        "nonfinite input does not enter geometry construction");
  const auto bounded = BuildSurfaceRibbon(axis, start, end, {.AxisPoints = 2});
  CHECK(!bounded && bounded.error() == SurfaceRibbonError::CapacityExceeded,
        "input work is bounded before the axis copy");
  std::stop_source stop;
  stop.request_stop();
  const auto canceled = BuildSurfaceRibbon(axis, start, end, {}, stop.get_token());
  CHECK(!canceled && canceled.error() == SurfaceRibbonError::Canceled,
        "cancellation publishes no partial domain");
}

void SharpTurn() {
  const std::array<PlanarPoint, 3> axis{{{0, 0}, {4, 0}, {0, 1}}};
  const std::array<PlanarPoint, 3> first{{{0, -1}, {0, 0}, {0, 1}}};
  const double length = std::sqrt(17.0);
  const std::array<PlanarPoint, 3> last{
      {{1 / length, 1 + 4 / length}, {0, 1}, {-1 / length, 1 - 4 / length}}};
  const auto turn = BuildSurfaceRibbon(axis,
                                       {.StationM = 0, .Profile = first, .AxisVertex = 1},
                                       {.StationM = 4 + length, .Profile = last, .AxisVertex = 1});
  CHECK(!turn && turn.error() == SurfaceRibbonError::UnsupportedTurn,
        "a near-reversing source needs another recipe instead of an unbounded miter");
}
}

int main() {
  Straight();
  Bend();
  SharpTurn();
  return outshine::Test::Report();
}
