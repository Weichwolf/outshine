#include "Check.h"
#include "src/generators/road/JunctionFootprint.h"
#include "src/generators/road/ProfiledRoadMesher.h"

#include <array>
#include <cmath>
#include <numbers>
#include <vector>

namespace {
outshine::RoadGate Gate(double angle, double width = 3) {
  const double east = std::cos(angle), north = std::sin(angle);
  return {.EastM = 8 * east,
          .NorthM = 8 * north,
          .GradeM = 5 + .07 * 8 * east - .03 * 8 * north,
          .OutE = east,
          .OutN = north,
          .HalfWidthM = width};
}

double RimArea(const outshine::Generators::JunctionFootprint &footprint) {
  double twice = 0;
  for (size_t at = 0; at < footprint.Rim.size(); ++at) {
    const auto &a = footprint.Rim[at];
    const auto &b = footprint.Rim[(at + 1) % footprint.Rim.size()];
    twice += a.EastM * b.NorthM - a.NorthM * b.EastM;
  }
  return std::abs(twice) / 2;
}

void CheckJunction(std::span<const outshine::RoadGate> gates, double expectedArea = 0) {
  using namespace outshine;
  using namespace outshine::Test;
  const auto footprint = Generators::BuildJunctionFootprint(gates);
  const double footprintArea = RimArea(footprint);
  if (expectedArea > 0) {
    CHECK_NEAR(footprintArea, expectedArea, 1e-8, "square metres", "complete gate footprint");
  }
  Generators::ProfiledRoadMesher mesher;
  RoadMeshBuffers mesh;
  mesher.Junction(gates, {.SlopeE = .07, .SlopeN = -.03}, {{.2f, .3f, .4f}}, mesh);
  double topArea = 0;
  for (size_t at = 0; at < mesh.PositionM.size(); at += 3) {
    if (mesh.NormalM[at + 1] <= 0) { continue; }
    const double height = 5 + .07 * mesh.PositionM[at] + .03 * mesh.PositionM[at + 2];
    CHECK_NEAR(
        mesh.PositionM[at + 1],
        height,
        .00002,
        "metres",
        "stored horizontal positions determine the shared plane without millimetre height steps");
  }
  for (size_t at = 0; at < mesh.Index.size(); at += 3) {
    const size_t a = mesh.Index[at] * 3, b = mesh.Index[at + 1] * 3, c = mesh.Index[at + 2] * 3;
    std::array<double, 3> u{}, v{}, cross{};
    for (size_t axis = 0; axis < 3; ++axis) {
      u[axis] = static_cast<double>(mesh.PositionM[b + axis]) - mesh.PositionM[a + axis];
      v[axis] = static_cast<double>(mesh.PositionM[c + axis]) - mesh.PositionM[a + axis];
    }
    for (size_t axis = 0; axis < 3; ++axis) {
      cross[axis] = u[(axis + 1) % 3] * v[(axis + 2) % 3] - u[(axis + 2) % 3] * v[(axis + 1) % 3];
    }
    double facing = 0, length = 0;
    for (size_t axis = 0; axis < 3; ++axis) {
      facing += cross[axis] * mesh.NormalM[a + axis];
      length += cross[axis] * cross[axis];
    }
    CHECK(length > 1e-12 && facing > 0, "every junction face is nondegenerate and outward");
    if (mesh.NormalM[a + 1] > 0) { topArea += std::abs(cross[1]) / 2; }
  }
  CHECK_NEAR(topArea, footprintArea, .06, "square metres", "pavement covers the entire bed");
  for (const auto &corner : footprint.Rim) {
    bool found = false;
    for (size_t at = 0; at < mesh.PositionM.size(); at += 3) {
      found |=
          std::abs(mesh.PositionM[at] - corner.EastM) < .002 &&
          std::abs(mesh.PositionM[at + 2] + corner.NorthM) < .002 &&
          std::abs(mesh.PositionM[at + 1] - (5 + .07 * corner.EastM - .03 * corner.NorthM)) < .002;
    }
    CHECK(found, "every gate corner belongs to the pavement on the shared grade plane");
  }
}
}

int main() {
  using namespace outshine;
  using namespace outshine::Test;
  constexpr double pi = std::numbers::pi;
  for (const double bend : {0., pi / 6, pi / 3, pi / 2}) {
    const std::array gates{Gate(0), Gate(pi - bend)};
    CheckJunction(gates, bend == 0 ? 96 : 0);
    const std::array reversed{gates[1], gates[0]};
    CheckJunction(reversed);
  }
  CheckJunction(std::array{Gate(0), Gate(pi / 2), Gate(pi)}, 151);
  CheckJunction(std::array{Gate(0), Gate(pi / 2), Gate(pi), Gate(3 * pi / 2)}, 206);
  CheckJunction(std::array{Gate(.3, 2), Gate(1.8, 4), Gate(3.1, 3), Gate(4.9, 2)});
  CHECK(Generators::BuildJunctionFootprint({}).Rim.empty(), "no gates produce no footprint");
  return Report();
}
