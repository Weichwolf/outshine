#include "PolygonTriangulation.h"
#include "Check.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <vector>

int main() {
  using namespace outshine;
  using namespace outshine::Test;
  const std::vector<PolygonRing> polygon{{{0, 0}, {10, 0}, {10, 4}, {6, 4}, {6, 10}, {0, 10}},
                                         {{1, 1}, {3, 1}, {3, 3}, {1, 3}},
                                         {{1, 6}, {3, 6}, {3, 8}, {1, 8}}};
  for (const bool reverse : {false, true}) {
    auto rings = polygon;
    if (reverse) {
      for (auto &ring : rings) {
        std::ranges::reverse(ring);
        ring.push_back(ring.front());
      }
    }
    const auto triangles = TriangulatePolygon(rings);
    CHECK(triangles.has_value(), "concave shell and two holes accept either winding and closure");
    if (!triangles) { continue; }
    CHECK(triangles->size() == 48, "14 unique corners and two holes produce sixteen triangles");
    std::vector<std::array<double, 2>> points;
    for (const auto &ring : rings) { points.insert(points.end(), ring.begin(), ring.end()); }
    double area = 0;
    bool inside = true;
    for (size_t at = 0; at < triangles->size(); at += 3) {
      const auto &a = points[(*triangles)[at]];
      const auto &b = points[(*triangles)[at + 1]];
      const auto &c = points[(*triangles)[at + 2]];
      area += 0.5 * std::abs((b[0] - a[0]) * (c[1] - a[1]) - (b[1] - a[1]) * (c[0] - a[0]));
      const double x = (a[0] + b[0] + c[0]) / 3;
      const double y = (a[1] + b[1] + c[1]) / 3;
      inside = inside && x >= 0 && y >= 0 && x <= 10 && y <= 10 && (x <= 6 || y <= 4) &&
               !(x > 1 && x < 3 && ((y > 1 && y < 3) || (y > 6 && y < 8)));
    }
    CHECK_NEAR(area,
               68,
               1e-12,
               "polygon area",
               "the L-shaped 76 square metre shell retains both four square metre holes");
    CHECK(inside, "all triangle centroids remain in the independently described occupied domain");
  }
  const std::array<PolygonRing, 1> crossed{{{{0, 0}, {4, 4}, {0, 4}, {4, 0}}}};
  auto outside = polygon;
  outside[1] = {{20, 20}, {21, 20}, {21, 21}, {20, 21}};
  auto overlap = polygon;
  overlap[2] = {{2, 2}, {4, 2}, {4, 4}, {2, 4}};
  auto nonfinite = polygon;
  nonfinite[0][0][0] = std::numeric_limits<double>::infinity();
  for (const auto &rings : {std::vector<PolygonRing>{},
                            std::vector<PolygonRing>{crossed[0]},
                            outside,
                            overlap,
                            nonfinite}) {
    const auto triangles = TriangulatePolygon(rings);
    CHECK(!triangles && triangles.error() == PolygonTriangulationError::InvalidPolygon,
          "missing, crossing, exterior, overlapping and nonfinite contours are rejected");
  }
  return Report();
}
