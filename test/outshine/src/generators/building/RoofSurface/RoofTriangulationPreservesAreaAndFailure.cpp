#include "RoofSurface.h"
#include "Check.h"
#include <array>
#include <cmath>
#include <limits>
#include <vector>

int main() {
  using namespace outshine::Generators;
  using namespace outshine::Test;
  const std::array<En, 4> rectangle{{{0, 0}, {4, 0}, {4, 4}, {0, 4}}};
  const std::array<En, 6> concave{{{0, 0}, {4, 0}, {4, 1}, {1, 1}, {1, 4}, {0, 4}}};
  BuildingScratch scratch;
  std::vector<En> triangles;
  CHECK(RoofSurface::Fill(rectangle, scratch, triangles), "convex polygon triangulates");
  const std::array<size_t, 6> order{3, 0, 1, 3, 1, 2};
  CHECK(triangles.size() == order.size(), "rectangle produces two triangles");
  for (size_t at = 0; at < triangles.size() && at < order.size(); ++at) {
    CHECK(triangles[at].EastM == rectangle[order[at]].EastM &&
              triangles[at].NorthM == rectangle[order[at]].NorthM,
          "deterministic rectangle triangulation order preserved");
  }
  triangles.clear();
  CHECK(RoofSurface::Fill(concave, scratch, triangles), "concave footprint triangulates");
  CHECK(triangles.size() == 12, "six-corner polygon produces four triangles");
  double area = 0;
  for (size_t at = 0; at + 2 < triangles.size(); at += 3) {
    const auto &a = triangles[at];
    const auto &b = triangles[at + 1];
    const auto &c = triangles[at + 2];
    const double twice =
        (b.EastM - a.EastM) * (c.NorthM - a.NorthM) - (b.NorthM - a.NorthM) * (c.EastM - a.EastM);
    CHECK(twice > 0, "all triangles retain counterclockwise orientation");
    area += twice / 2;
    CHECK((a.EastM + b.EastM + c.EastM) / 3 <= 1 || (a.NorthM + b.NorthM + c.NorthM) / 3 <= 1,
          "triangle center does not fill the concave notch");
  }
  CHECK(area == 7, "two four-unit strips minus their shared unit square have area seven");
  const auto previous = triangles;
  const auto preserved = [&] {
    CHECK(triangles.size() == previous.size(), "failure preserves triangle count");
    for (size_t at = 0; at < triangles.size() && at < previous.size(); ++at) {
      CHECK(triangles[at].EastM == previous[at].EastM &&
                triangles[at].NorthM == previous[at].NorthM,
            "failure preserves previous triangle coordinates");
    }
  };
  for (const double value :
       {std::numeric_limits<double>::infinity(), std::numeric_limits<double>::quiet_NaN()}) {
    for (size_t corner = 0; corner < rectangle.size(); ++corner) {
      auto invalid = rectangle;
      invalid[corner].EastM = value;
      CHECK(!RoofSurface::Fill(invalid, scratch, triangles), "nonfinite easting rejected");
      preserved();
      invalid = rectangle;
      invalid[corner].NorthM = value;
      CHECK(!RoofSurface::Fill(invalid, scratch, triangles), "nonfinite northing rejected");
      preserved();
    }
  }
  const double maximum = std::numeric_limits<double>::max();
  const std::array<En, 3> overflowing{{{0, 0}, {maximum, 0}, {0, maximum}}};
  CHECK(!RoofSurface::Fill(overflowing, scratch, triangles), "overflowing orientation rejected");
  preserved();
  const std::array<En, 3> clockwise{{{0, 0}, {0, 1}, {1, 0}}};
  CHECK(!RoofSurface::Fill(clockwise, scratch, triangles), "unsupported winding rejected");
  preserved();
  CHECK(!RoofSurface::Fill({}, scratch, triangles), "empty polygon rejected");
  preserved();
  return Report();
}
