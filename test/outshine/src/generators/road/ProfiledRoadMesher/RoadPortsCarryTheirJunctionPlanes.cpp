#include "Check.h"
#include "src/generators/road/ProfiledRoadMesher.h"

#include <array>
#include <cmath>

namespace {

double Height(outshine::RoadPlane plane, double east, double north) {
  return 5.0 + plane.SlopeE * east + plane.SlopeN * north;
}

void CheckPortedRoad(std::span<const outshine::RoadStation> stations, outshine::RoadPlane plane) {
  using namespace outshine;
  using namespace outshine::Test;
  RoadMeshBuffers mesh;
  const auto result = Generators::ProfiledRoadMesher{}.Sweep(stations,
                                                             {.HalfWidthM = 2,
                                                              .Profile = RoadProfile::Simple,
                                                              .WearsLinear = {{.2f, .3f, .4f}},
                                                              .Crossfall = .04,
                                                              .Form = RibbonForm::Surface,
                                                              .EndPlanes = {plane, plane}},
                                                             mesh);
  CHECK(result.Pieces == 1 && result.Cuts == 0 && result.Refused == 0,
        "ported road forms one complete fitted surface");
  const double normalLength = std::hypot(plane.SlopeE, 1.0, plane.SlopeN);
  const std::array expectedNormal{
      -plane.SlopeE / normalLength, 1.0 / normalLength, plane.SlopeN / normalLength};
  for (size_t end = 0; end < 2; ++end) {
    const RoadStation &station = end == 0 ? stations.front() : stations.back();
    const RoadStation &adjacent = end == 0 ? stations[1] : stations[stations.size() - 2];
    const double east = station.EastM - adjacent.EastM;
    const double north = station.NorthM - adjacent.NorthM;
    const double run = std::hypot(east, north);
    size_t found = 0;
    for (size_t at = 0; at < mesh.PositionM.size(); at += 3) {
      const double along = ((mesh.PositionM[at] - station.EastM) * east +
                            (-mesh.PositionM[at + 2] - station.NorthM) * north) /
                           run;
      if (std::abs(along) > .0001) { continue; }
      CHECK_NEAR(mesh.PositionM[at + 1],
                 Height(plane, mesh.PositionM[at], -mesh.PositionM[at + 2]),
                 .00002,
                 "m",
                 "every endpoint section vertex lies on the junction plane");
      for (size_t axis = 0; axis < 3; ++axis) {
        CHECK_NEAR(mesh.NormalM[at + axis],
                   expectedNormal[axis],
                   .00001,
                   "unit",
                   "endpoint surface normal matches the complete junction plane");
      }
      ++found;
    }
    CHECK(found == kRibbonAcross, "complete endpoint section is checked");
  }
}

}

int main() {
  using namespace outshine;
  constexpr RoadPlane plane{.SlopeE = .06, .SlopeN = -.03};
  for (const double heading : {0.0, .7, 1.5707963267948966, -2.0}) {
    const double east = std::cos(heading), north = std::sin(heading);
    const std::array<RoadStation, 2> stations{
        {{.EastM = 13.4567, .NorthM = -25.6789, .GradeM = Height(plane, 13.4567, -25.6789)},
         {.EastM = 13.4567 + 40 * east,
          .NorthM = -25.6789 + 40 * north,
          .GradeM = Height(plane, 13.4567 + 40 * east, -25.6789 + 40 * north)}}};
    CheckPortedRoad(stations, plane);
  }
  const std::array<RoadStation, 3> bend{
      {{.EastM = 0, .NorthM = 0, .GradeM = Height(plane, 0, 0)},
       {.EastM = 50, .NorthM = 0, .GradeM = Height(plane, 50, 0)},
       {.EastM = 100, .NorthM = 15, .GradeM = Height(plane, 100, 15)}}};
  CheckPortedRoad(bend, plane);
  return Test::Report();
}
