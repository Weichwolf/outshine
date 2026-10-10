#include "Check.h"
#include "src/generators/road/ProfiledRoadMesher.h"
#include "src/generators/road/RoadCrossSection.h"

#include <algorithm>
#include <array>
#include <cmath>

namespace {

bool HasVertex(const outshine::RoadMeshBuffers &mesh,
               outshine::Vec3f position,
               outshine::Vec3f normal) {
  for (size_t at = 0; at < mesh.PositionM.size(); at += 3) {
    bool same = true;
    for (size_t axis = 0; axis < 3; ++axis) {
      same &=
          mesh.PositionM[at + axis] == position[axis] && mesh.NormalM[at + axis] == normal[axis];
    }
    if (same) { return true; }
  }
  return false;
}

bool HasSurfaceEdge(const outshine::RoadMeshBuffers &mesh, outshine::Vec3f a, outshine::Vec3f b) {
  const auto matches = [&](uint32_t vertex, outshine::Vec3f position) {
    const size_t at = static_cast<size_t>(vertex) * 3;
    return mesh.NormalM[at + 1] > 0 && mesh.PositionM[at] == position[0] &&
           mesh.PositionM[at + 1] == position[1] && mesh.PositionM[at + 2] == position[2];
  };
  for (size_t at = 0; at < mesh.Index.size(); at += 3) {
    for (size_t side = 0; side < 3; ++side) {
      const uint32_t p = mesh.Index[at + side], q = mesh.Index[at + (side + 1) % 3];
      if ((matches(p, a) && matches(q, b)) || (matches(p, b) && matches(q, a))) { return true; }
    }
  }
  return false;
}

void CheckPorts(outshine::RoadProfile profile, outshine::RibbonForm form, bool reversed) {
  using namespace outshine;
  using namespace outshine::Test;
  const Generators::ProfiledRoadMesher mesher;
  const RoadPlane plane{.SlopeE = .07, .SlopeN = -.03};
  const Section section = Generators::RoadSection(3, profile);
  const auto grade = [](double east, double north) {
    return 503.234567 + .07 * east - .03 * north;
  };
  std::array<RoadGate, 4> gates;
  for (size_t at = 0; at < gates.size(); ++at) {
    const double angle = .37 + static_cast<double>(at) * 1.5707963267948966;
    const double east = std::cos(angle), north = std::sin(angle);
    const double e = 8003.123456 + 8 * east, n = -7043.654321 + 8 * north;
    gates[at] = {.EastM = e,
                 .NorthM = n,
                 .GradeM = grade(e, n),
                 .OutE = east,
                 .OutN = north,
                 .HalfWidthM = section.HalfWidthM,
                 .ShoulderM = section.ShoulderM,
                 .ThicknessM = section.ThicknessM};
  }
  RoadMeshBuffers junction;
  mesher.Junction(gates, plane, {{.2f, .3f, .4f}}, junction);
  CHECK(!junction.Index.empty(), "four complete junction ports produce a surface");
  const Vec3f normal = Generators::RoadPortNormal(plane);
  for (const auto &gate : gates) {
    std::array stations{
        RoadStation{gate.EastM, gate.NorthM, gate.GradeM},
        RoadStation{gate.EastM + gate.OutE * 20,
                    gate.NorthM + gate.OutN * 20,
                    grade(gate.EastM + gate.OutE * 20, gate.NorthM + gate.OutN * 20)}};
    if (reversed) { std::ranges::reverse(stations); }
    RoadSweep how{.HalfWidthM = section.HalfWidthM,
                  .Profile = profile,
                  .WearsLinear = {{.2f, .3f, .4f}},
                  .Form = form};
    const RoadPort port{gate, plane};
    how.EndPorts[reversed ? 1 : 0] = port;
    RoadMeshBuffers road;
    const auto result = mesher.Sweep(stations, how, road);
    CHECK(result.Pieces == 1 && result.Refused == 0, "port keeps its complete road approach");
    const auto positions = Generators::RoadPortPositions(port);
    for (size_t across = 0; across + 1 < positions.size(); ++across) {
      CHECK(HasSurfaceEdge(road, positions[across], positions[across + 1]) &&
                HasSurfaceEdge(junction, positions[across], positions[across + 1]),
            "both surfaces triangulate the same complete port segments without T junctions");
    }
    for (const auto &position : positions) {
      CHECK(HasVertex(road, position, normal) && HasVertex(junction, position, normal),
            "all four stored position and normal triples match exactly at the junction");
      if (form != RibbonForm::ClosedShell) { continue; }
      Vec3f floor = position, down = normal;
      for (size_t axis = 0; axis < 3; ++axis) {
        floor[axis] = static_cast<float>(position[axis] - section.ThicknessM * normal[axis]);
        down[axis] = -down[axis];
      }
      CHECK(HasVertex(road, floor, down) && HasVertex(junction, floor, down),
            "bridge ports share the bottom surface across their full section");
    }
  }
}

}

int main() {
  for (const auto profile : {outshine::RoadProfile::Simple,
                             outshine::RoadProfile::Kerbed,
                             outshine::RoadProfile::Rounded}) {
    for (const auto form : {outshine::RibbonForm::Surface, outshine::RibbonForm::ClosedShell}) {
      for (const bool reversed : {false, true}) { CheckPorts(profile, form, reversed); }
    }
  }
  return outshine::Test::Report();
}
