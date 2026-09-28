#include "TriangleRegion.h"
#include "Check.h"
#include <array>
#include <cmath>
#include <limits>

int main() {
  using namespace outshine;
  using namespace outshine::Test;
  const PointEnclosure a{.EstimateM = {{0, 0, 0}}, .RadiusM = 2};
  const PointEnclosure b{.EstimateM = {{2, 0, 0}}, .RadiusM = 4};
  const auto mid = EnclosePointBlend({a, b}, 0.5);
  CHECK(mid && mid->EstimateM[0] == 1 && mid->RadiusM >= 3 && mid->RadiusM < 3 + 1e-12,
        "convex midpoint propagates both input uncertainty balls");
  const auto start = EnclosePointBlend({a, b}, 0);
  const auto end = EnclosePointBlend({a, b}, 1);
  CHECK(start && end && start->RadiusM == a.RadiusM && end->RadiusM == b.RadiusM &&
            start->EstimateM[0] == 0 && end->EstimateM[0] == 2,
        "endpoint selection preserves an existing enclosure exactly");
  constexpr double huge = 0x1p53;
  const auto rounded = EnclosePointBlend({PointEnclosure{.EstimateM = {{huge, 0, 0}}},
                                          PointEnclosure{.EstimateM = {{huge + 2, 0, 0}}}},
                                         0.5);
  CHECK(rounded && rounded->RadiusM >= 1 && rounded->RadiusM < 16,
        "nonrepresentable exact midpoint is covered rather than treated as exact");
  const std::array vertices{PointEnclosure{.EstimateM = {{0, 0, 0}}, .RadiusM = 0.1},
                            PointEnclosure{.EstimateM = {{3, 0, 0}}, .RadiusM = 0.2},
                            PointEnclosure{.EstimateM = {{0, 3, 0}}, .RadiusM = 0.3}};
  const auto region = EncloseTriangleRegion(vertices);
  CHECK(region && std::abs(region->InteriorSample.EstimateM[0] - 1) < 1e-15 &&
            std::abs(region->InteriorSample.EstimateM[1] - 1) < 1e-15 &&
            region->InteriorSample.RadiusM >= 0.2,
        "an interior convex sample encloses input uncertainty and arithmetic rounding");
  CHECK(region && region->RadiusM >= std::sqrt(5.0) + 0.3 &&
            region->RadiusM < std::sqrt(5.0) + 0.3 + 1e-12,
        "radius covers true vertices and every convex triangle interior point");
  auto negative = a;
  negative.RadiusM = -1;
  CHECK(!EnclosePointBlend({negative, b}, 0.5), "negative uncertainty is refused");
  auto nonfinite = a;
  nonfinite.EstimateM[2] = std::numeric_limits<double>::infinity();
  CHECK(!EnclosePointBlend({nonfinite, b}, 0.5), "nonfinite coordinates are refused");
  nonfinite = a;
  nonfinite.RadiusM = std::numeric_limits<double>::infinity();
  CHECK(!EnclosePointBlend({nonfinite, b}, 0.5), "unbounded uncertainty is refused");
  CHECK(!EnclosePointBlend({a, b}, -0.1) && !EnclosePointBlend({a, b}, 1.1) &&
            !EnclosePointBlend({a, b}, std::numeric_limits<double>::quiet_NaN()),
        "nonconvex and nonfinite weights are refused");
  auto invalidVertices = vertices;
  invalidVertices[2] = negative;
  CHECK(!EncloseTriangleRegion(invalidVertices), "invalid triangle vertex cannot be ignored");
  const double maximum = std::numeric_limits<double>::max();
  CHECK(!EnclosePointBlend({PointEnclosure{.EstimateM = {{maximum, 0, 0}}},
                            PointEnclosure{.EstimateM = {{-maximum, 0, 0}}}},
                           0.5),
        "unrepresentable rounding enclosure fails closed");
  return Report();
}
