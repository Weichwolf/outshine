#include <algorithm>
#include <array>
#include <cmath>
#include <map>
#include <utility>
#include <vector>
#include "../../../src/base/curve/Ribbon.h"
#include "Check.h"

int main() {
  using namespace outshine;
  using namespace outshine::Test;
  ReferenceLine line;
  std::string error;
  CHECK(line.Lay({}, {{.LengthM = 20}}, error), "straight reference");
  CHECK(line.Rise({{.AlongM = 0, .Value = 0, .RatePerM = 0.25},
                   {.AlongM = 20, .Value = 5, .RatePerM = 0.25}},
                  error),
        "constant grade");
  CHECK(line.Bank({{.AlongM = 0, .Value = 0.12}, {.AlongM = 20, .Value = 0.12}}, error),
        "constant bank");
  for (double shoulder : {0.0, 1.0}) {
    const auto ribbon =
        Sweep(line, {.HalfWidthM = 3, .ShoulderM = shoulder, .ThicknessM = 0.5}, 0, 20, 2);
    CHECK(ribbon.Woven, "both declared cross sections produce a shell");
    std::map<std::array<float, 3>, size_t> welded;
    std::vector<size_t> vertexIds;
    for (size_t at = 0; at + 2 < ribbon.PositionM.size(); at += 3) {
      const std::array<float, 3> point = {
          ribbon.PositionM[at], ribbon.PositionM[at + 1], ribbon.PositionM[at + 2]};
      const auto [where, inserted] = welded.try_emplace(point, welded.size());
      (void)inserted;
      vertexIds.push_back(where->second);
    }

    struct Use {
      int Count = 0;
      int Orientation = 0;
    };

    std::map<std::pair<size_t, size_t>, Use> edges;
    double volume = 0;
    for (size_t at = 0; at + 2 < ribbon.Index.size(); at += 3) {
      std::array<Vec3, 3> points;
      for (size_t corner = 0; corner < 3; ++corner) {
        const size_t vertex = ribbon.Index[at + corner];
        for (size_t axis = 0; axis < 3; ++axis) {
          points[corner][axis] = ribbon.PositionM[vertex * 3 + axis];
        }
        const size_t a = vertexIds[vertex];
        const size_t b = vertexIds[ribbon.Index[at + (corner + 1) % 3]];
        CHECK(a != b, "every triangle edge spans distinct geometric vertices");
        auto &use = edges[{std::min(a, b), std::max(a, b)}];
        ++use.Count;
        use.Orientation += a < b ? 1 : -1;
      }
      Vec3 ab;
      Vec3 ac;
      for (size_t axis = 0; axis < 3; ++axis) {
        ab[axis] = points[1][axis] - points[0][axis];
        ac[axis] = points[2][axis] - points[0][axis];
      }
      const auto normal = Cross(ab, ac);
      CHECK(std::hypot(normal[0], normal[1], normal[2]) > 0, "no zero-area triangles");
      const auto crossed = Cross(points[1], points[2]);
      for (size_t axis = 0; axis < 3; ++axis) { volume += points[0][axis] * crossed[axis] / 6; }
    }
    for (const auto &[edge, use] : edges) {
      (void)edge;
      CHECK(use.Count == 2 && use.Orientation == 0,
            "every welded edge belongs to exactly two oppositely oriented faces");
    }
    const double bank = std::tan(0.12);
    const double expectedVolume =
        2 * (3 + shoulder) * 20 * 0.5 * std::sqrt(1 + 0.25 * 0.25 + bank * bank);
    CHECK_NEAR(
        volume,
        expectedVolume,
        expectedVolume * 1e-6,
        "m3",
        "closed prism volume matches surface area times normal thickness at float precision");
  }
  return Report();
}
