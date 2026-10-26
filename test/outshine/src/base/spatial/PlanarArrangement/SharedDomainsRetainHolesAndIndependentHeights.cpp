#include "Check.h"
#include "PlanarArrangement.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace {
using namespace outshine;
using namespace outshine::Test;
constexpr double kAreaTolerance = 1e-12;

double Area(const PlanarMesh &mesh, PlanarFace face) {
  const auto a = mesh.Points[face[0]];
  const auto b = mesh.Points[face[1]];
  const auto c = mesh.Points[face[2]];
  return ((b[0] - a[0]) * (c[1] - a[1]) - (b[1] - a[1]) * (c[0] - a[0])) / 2;
}

void HolesAndOwnership() {
  const std::array domains{
      PlanarDomain{.Rings = {{{0, 0}, {8, 0}, {8, 8}, {0, 8}}, {{2, 2}, {2, 4}, {4, 4}, {4, 2}}}},
      PlanarDomain{.Rings = {{{6, -2}, {10, -2}, {10, 4}, {6, 4}}}}};
  const std::array lines{PlanarLine{{-1, 1}, {11, 1}}, PlanarLine{{7, -3}, {7, 9}}};
  const auto result = ArrangePlanarDomains(
      domains, lines, {.WeldDistanceM = 0, .MaximumAreaDifferenceM2 = kAreaTolerance});
  CHECK(result.has_value(), "noded constraints triangulate overlapping domains with a hole");
  if (!result) { return; }
  std::array<double, 4> areas{};
  bool noCrossing = true;
  for (size_t at = 0; at < result->Mesh.Faces.size(); ++at) {
    const auto face = result->Mesh.Faces[at];
    const double area = Area(result->Mesh, face);
    CHECK(area > 0, "every retained triangle has positive winding and nonzero area");
    areas[result->FaceDomains[at]] += area;
    for (size_t axis = 0; axis < 2; ++axis) {
      const double station = axis == 0 ? 7 : 1;
      std::array<double, 3> values{};
      for (size_t corner = 0; corner < 3; ++corner) {
        values[corner] = result->Mesh.Points[face[corner]][axis];
      }
      noCrossing &= (*std::ranges::min_element(values) >= station ||
                     *std::ranges::max_element(values) <= station);
    }
  }
  CHECK_NEAR(areas[1],
             52,
             kAreaTolerance,
             "first-only area",
             "the hole and overlap retain their source ownership");
  CHECK_NEAR(
      areas[2], 16, kAreaTolerance, "second-only area", "the external source portion is retained");
  CHECK_NEAR(areas[3],
             8,
             kAreaTolerance,
             "shared area",
             "overlap is drawn once with two independent owners");
  CHECK(noCrossing, "no triangle crosses either noded interface constraint");
  CHECK(result->Work.MaximumAreaDifferenceM2 == 0,
        "each independently owned footprint has zero symmetric area difference");
}

void SeparateLayersAndGap() {
  const PlanarDomain triangle{.Rings = {{{0, 0}, {2, 0}, {0, 2}}}};
  const std::array stacked{triangle, triangle};
  const auto same = ArrangePlanarDomains(stacked, {}, {});
  CHECK(same && same->Mesh.Faces.size() == 1 && same->FaceDomains[0] == 3,
        "equal XY footprints keep both owners without joining their height variables");
  const std::array separated{triangle, PlanarDomain{.Rings = {{{0, -1e-5}, {2, -1e-5}, {0, -2}}}}};
  const auto gap = ArrangePlanarDomains(
      separated, {}, {.WeldDistanceM = 1e-8, .MaximumAreaDifferenceM2 = kAreaTolerance});
  CHECK(gap && gap->Mesh.Points.size() == 6 && gap->Mesh.Faces.size() == 2 &&
            std::ranges::none_of(gap->FaceDomains, [](uint64_t owners) { return owners == 3; }),
        "a real gap beyond the explicit weld distance stays open");
}
}

int main() {
  HolesAndOwnership();
  SeparateLayersAndGap();
  return Report();
}
