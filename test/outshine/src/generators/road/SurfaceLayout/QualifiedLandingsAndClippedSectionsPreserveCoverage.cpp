#include "Check.h"
#include "SurfaceLayout.h"
#include <array>
#include <cmath>
#include <cstdint>
#include <optional>
#include <stop_token>
#include <vector>

namespace {
using namespace outshine;
using namespace outshine::Generators;
constexpr double kAreaErrorM2 = 1e-7;

double Area(const PlanarMesh &mesh) {
  double area = 0;
  for (const auto &face : mesh.Faces) {
    const auto &a = mesh.Points[face[0]];
    const auto &b = mesh.Points[face[1]];
    const auto &c = mesh.Points[face[2]];
    area += std::abs((b[0] - a[0]) * (c[1] - a[1]) - (b[1] - a[1]) * (c[0] - a[0])) / 2;
  }
  return area;
}

void Landing() {
  const PlanarDomain stair{.Rings = {{{-.75, -4}, {.75, -4}, {.75, 0}, {-.75, 0}}}};
  const PlanarDomain path{.Rings = {{{-6, -1}, {6, -1}, {6, 1}, {-6, 1}}}};
  const std::array<PlanarPoint, 2> stairAxis{{{0, -4}, {0, 0}}};
  const std::array<PlanarPoint, 2> pathAxis{{{-6, 0}, {6, 0}}};
  const std::array<SurfaceLayoutSource, 2> sources{
      {{.Domain = stair, .Axis = stairAxis, .WidthM = 1.5, .Steps = true},
       {.Domain = path, .Axis = pathAxis, .WidthM = 2}}};
  const std::array<uint32_t, 2> owners{0, 1};
  const std::array<SurfaceLanding, 1> landings{{{.Origin = {0, 0}, .Sources = owners}}};
  const auto layout = BuildSurfaceLayout(sources, landings);
  CHECK(layout.has_value(), "declared source incidence produces one native planar layout");
  if (!layout) { return; }
  CHECK(layout->Ports.size() == 1 && layout->Ports[0].Domains == std::vector<uint32_t>({0, 1}),
        "the landing retains qualified source owners in their declared order");
  CHECK(layout->Sections[0].Count == 0 && layout->Sections[1].Count == 5,
        "interior path sections do not replace the separate stair-flight construction");
  CHECK(layout->Work.SectionQueries == 5 && layout->Work.SectionParts == 5 &&
            layout->Work.LandingPairs == 1 && layout->Work.ConstraintQueries == 5 &&
            layout->Work.ConstraintEdgeTests > 0,
        "input-dependent section and incident-pair work is explicit");
  constexpr double kUnionAreaM2 = 28.5;
  CHECK(std::abs(Area(layout->Arrangement.Mesh) - kUnionAreaM2) < kAreaErrorM2,
        "the planar mesh preserves the union 6+24-1.5 square metres");
  const auto range = layout->Sections[1];
  for (uint32_t at = range.First; at < range.First + range.Count; ++at) {
    const auto &line = layout->Constraints[at];
    CHECK(
        line.size() == 2 && std::abs(line.front()[1]) == 1 && std::abs(line.back()[1]) == 1,
        "each section ends on its own source boundary instead of extending into the other source");
  }
  const auto absent = BuildSurfaceLayout(sources, {}, {.SectionSpacingM = std::nullopt});
  CHECK(absent && absent->Ports.empty() && absent->Constraints.empty(),
        "overlap alone creates neither a declared join nor a requested section");
  const auto pairLimit = BuildSurfaceLayout(sources, landings, {}, {.LandingPairs = 0});
  CHECK(!pairLimit &&
            std::get<SurfaceLayoutError>(pairLimit.error()) == SurfaceLayoutError::CapacityExceeded,
        "the incident-pair limit rejects work before constructing a partial landing");
  const auto edgeLimit = BuildSurfaceLayout(sources, landings, {}, {.ConstraintCandidates = 0});
  CHECK(!edgeLimit &&
            std::get<SurfaceLayoutError>(edgeLimit.error()) == SurfaceLayoutError::CapacityExceeded,
        "section coverage cannot publish unchecked constraints after exhausting its edge budget");
  const std::array<uint32_t, 2> duplicate{0, 0};
  const std::array<SurfaceLanding, 1> invalid{{{.Origin = {0, 0}, .Sources = duplicate}}};
  const auto repeated = BuildSurfaceLayout(sources, invalid);
  CHECK(!repeated &&
            std::get<SurfaceLayoutError>(repeated.error()) == SurfaceLayoutError::InvalidInput,
        "a repeated source cannot masquerade as a second landing owner");
}

void Hole() {
  const PlanarDomain domain{
      .Rings = {{{-5, -3}, {5, -3}, {5, 3}, {-5, 3}}, {{-1, -.5}, {-1, .5}, {1, .5}, {1, -.5}}}};
  const std::array<PlanarPoint, 2> axis{{{-4, -2}, {4, -2}}};
  const std::array<SurfaceLayoutSource, 1> sources{{{.Domain = domain, .Axis = axis, .WidthM = 6}}};
  const auto layout = BuildSurfaceLayout(sources, {});
  CHECK(layout && layout->Work.SectionQueries == 3 && layout->Sections[0].Count == 4,
        "a source hole splits a transverse section into independently clipped line parts");
  CHECK(layout && std::abs(Area(layout->Arrangement.Mesh) - 58) < kAreaErrorM2,
        "layout generation preserves the sixty square metre domain's two square metre hole");
  const auto badSpacing = BuildSurfaceLayout(sources, {}, {.SectionSpacingM = 0});
  CHECK(!badSpacing &&
            std::get<SurfaceLayoutError>(badSpacing.error()) == SurfaceLayoutError::InvalidInput,
        "invalid spacing is refused instead of entering an unbounded station loop");
  const auto limited = BuildSurfaceLayout(sources, {}, {}, {.InputCoordinates = 1});
  CHECK(!limited &&
            std::get<SurfaceLayoutError>(limited.error()) == SurfaceLayoutError::CapacityExceeded,
        "input coordinates are bounded before constructing source geometries");
  std::stop_source stop;
  stop.request_stop();
  const auto canceled = BuildSurfaceLayout(sources, {}, {}, {}, stop.get_token());
  CHECK(!canceled && std::get<SurfaceLayoutError>(canceled.error()) == SurfaceLayoutError::Canceled,
        "cancellation publishes no partial layout");
}
}

namespace {
void NumericCoverage() {
  const PlanarDomain domain{.Rings = {{{-5, -1}, {5, -1}, {5, 1}, {-5, 1}}}};
  constexpr double kSmallErrorM = SurfaceLayoutRecipe::kCoordinateMatchM / 4;
  const std::array<PlanarPoint, 2> nearby{{{-5 - kSmallErrorM, 0}, {5 + kSmallErrorM, 0}}};
  const std::array<SurfaceLayoutSource, 1> bounded{
      {{.Domain = domain, .Axis = nearby, .WidthM = 2}}};
  const auto accepted = BuildSurfaceLayout(bounded, {});
  CHECK(accepted && std::abs(Area(accepted->Arrangement.Mesh) - 20) < kAreaErrorM2,
        "source-axis construction error within the existing numeric bound does not enlarge the "
        "domain");
  constexpr double kLargeErrorM = 4 * SurfaceLayoutRecipe::kCoordinateMatchM;
  const std::array<PlanarPoint, 2> distant{{{-5 - kLargeErrorM, 0}, {5 + kLargeErrorM, 0}}};
  const std::array<SurfaceLayoutSource, 1> unbounded{
      {{.Domain = domain, .Axis = distant, .WidthM = 2}}};
  const auto refused = BuildSurfaceLayout(unbounded, {});
  CHECK(!refused &&
            std::get<SurfaceLayoutError>(refused.error()) == SurfaceLayoutError::InvalidInput,
        "a whole source axis outside the existing bound is refused");
  const PlanarDomain holed{
      .Rings = {{{-5, -2}, {5, -2}, {5, 2}, {-5, 2}}, {{-1, -1}, {-1, 1}, {1, 1}, {1, -1}}}};
  const std::array<PlanarPoint, 2> crossing{{{-4, 0}, {4, 0}}};
  const std::array<SurfaceLayoutSource, 1> throughHole{
      {{.Domain = holed, .Axis = crossing, .WidthM = 2}}};
  const auto gap = BuildSurfaceLayout(throughHole, {});
  CHECK(!gap && std::get<SurfaceLayoutError>(gap.error()) == SurfaceLayoutError::InvalidInput,
        "zero minimum distance cannot hide an axis crossing a genuine source hole");
}
}

int main() {
  Landing();
  Hole();
  NumericCoverage();
  return outshine::Test::Report();
}
