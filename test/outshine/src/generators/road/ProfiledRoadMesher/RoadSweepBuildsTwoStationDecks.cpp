#include "src/generators/road/ProfiledRoadMesher.h"
#include "Check.h"
#include <array>
#include <algorithm>
#include <cmath>

int main() {
  using namespace outshine;
  using namespace outshine::Test;
  Generators::ProfiledRoadMesher mesher;
  const RoadSweep settings{.HalfWidthM = 3,
                           .Profile = RoadProfile::Simple,
                           .WearsLinear = {{0.2f, 0.3f, 0.4f}},
                           .Crossfall = 0};
  for (double heading : {0.0, 0.7, -1.2}) {
    const double east = std::cos(heading), north = std::sin(heading);
    const std::array<RoadStation, 2> stations{
        {{.EastM = 10, .NorthM = 20, .GradeM = 5},
         {.EastM = 10 + 64 * east, .NorthM = 20 + 64 * north, .GradeM = 21}}};
    RoadMeshBuffers mesh;
    const auto result = mesher.Sweep(stations, settings, mesh);
    CHECK(result.Pieces == 1 && result.Refused == 0 && result.Cuts == 0,
          "two stations define one complete graded road deck");
    CHECK(!mesh.Index.empty() && !mesh.PositionM.empty(), "deck has indexed geometry");
    CHECK(mesh.NormalM.size() == mesh.PositionM.size() &&
              mesh.ColourRgba.size() / 4 == mesh.PositionM.size() / 3,
          "vertex streams agree");
    for (const auto &station : stations) {
      bool found = false;
      for (size_t at = 0; at < mesh.PositionM.size(); at += 3) {
        const double dx = mesh.PositionM[at] - station.EastM;
        const double dz = mesh.PositionM[at + 2] + station.NorthM;
        found |= std::abs(dx * east - dz * north) < 0.0001 &&
                 std::abs(std::hypot(dx, dz) - 3) < 0.0001 &&
                 std::abs(mesh.PositionM[at + 1] - station.GradeM) < 0.0001;
      }
      CHECK(found, "deck edge matches the declared endpoint, half-width and elevation");
    }
    const auto firstPositions = mesh.PositionM;
    const auto firstIndices = mesh.Index;
    const auto appended = mesher.Sweep(stations, settings, mesh);
    CHECK(appended.Pieces == 1 && mesh.PositionM.size() == 2 * firstPositions.size(),
          "append preserves the previous geometry");
    CHECK(std::equal(firstPositions.begin(), firstPositions.end(), mesh.PositionM.begin()),
          "existing vertices are unchanged");
    CHECK(mesh.Index.size() == 2 * firstIndices.size(), "appended index stream is complete");
    for (size_t at = 0; at < firstIndices.size(); ++at) {
      CHECK(mesh.Index[firstIndices.size() + at] == firstIndices[at] + firstPositions.size() / 3,
            "new indices address the appended vertices");
    }
  }
  RoadMeshBuffers empty;
  CHECK(mesher.Sweep({}, settings, empty).Pieces == 0 && empty.PositionM.empty(),
        "empty input cannot produce geometry");
  return Report();
}
