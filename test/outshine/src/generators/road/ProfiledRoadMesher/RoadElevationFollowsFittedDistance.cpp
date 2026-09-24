#include "src/generators/road/ProfiledRoadMesher.h"
#include "src/base/curve/Fit.h"
#include "Check.h"
#include <array>
#include <cmath>

int main() {
  using namespace outshine;
  using namespace outshine::Test;
  const std::array<double, 6> points{{0, 0, 100, 0, 200, 30}};
  ReferenceLine line;
  const auto fitted = Fit(points, 0.5, 5.5, line);
  CHECK(fitted.Laid, "gentle bend has a single drivable alignment");
  if (!fitted.Laid) { return Report(); }
  const double sourceLength = 100 + std::hypot(100.0, 30.0);
  CHECK(std::abs(sourceLength - line.LengthM()) > 0.0001,
        "fixture exercises a change in station distance");
  for (double sourceSlope : {0.25, -0.25}) {
    const std::array<RoadStation, 3> stations{
        {{.EastM = 0, .NorthM = 0, .GradeM = 7},
         {.EastM = 100, .NorthM = 0, .GradeM = 7 + sourceSlope * 100},
         {.EastM = 200, .NorthM = 30, .GradeM = 7 + sourceSlope * sourceLength}}};
    RoadMeshBuffers mesh;
    const auto result = Generators::ProfiledRoadMesher{}.Sweep(
        stations,
        {.HalfWidthM = 0.1, .Profile = RoadProfile::Simple, .WearsLinear = {{0.5f, 0.5f, 0.5f}}},
        mesh);
    CHECK(result.Pieces == 1 && result.Cuts == 0 && !mesh.Index.empty(),
          "graded bend produces a complete single deck");
    const double slope = sourceSlope * sourceLength / line.LengthM();
    const double normalLength = std::hypot(slope, 1.0);
    size_t checked = 0;
    for (size_t at = 0; at < mesh.PositionM.size(); at += 3) {
      if (std::abs(mesh.PositionM[at]) > 0.00001 || mesh.NormalM[at + 1] < 0.5) { continue; }
      CHECK(std::abs(mesh.NormalM[at] + slope / normalLength) < 0.000001 &&
                std::abs(mesh.NormalM[at + 1] - 1 / normalLength) < 0.000001 &&
                std::abs(mesh.NormalM[at + 2]) < 0.000001,
            "entry normal follows the chain rule for fitted station distance");
      ++checked;
    }
    CHECK(checked >= 2, "both deck edges expose the entry surface normal");
  }
  return Report();
}
