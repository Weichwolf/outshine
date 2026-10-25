#include "Check.h"
#include "JunctionFootprint.h"
#include "ProfiledRoadMesher.h"

#include <array>
#include <cmath>
#include <cstddef>
#include <span>
#include <numbers>

namespace {
inline constexpr double kSlopeE = .07;
inline constexpr double kSlopeN = .03;
inline constexpr double kAreaToleranceM2 = 1e-8;
inline constexpr double kHeightToleranceM = .00002;
inline constexpr double kSquaredAreaFloorM4 = 1e-12;
inline constexpr double kCoverageToleranceM2 = .06;
inline constexpr double kCornerToleranceM = .002;
inline constexpr double kStraightAreaM2 = 96;
inline constexpr double kTeeAreaM2 = 151;
inline constexpr double kCrossingAreaM2 = 206;
inline constexpr double kFirstAsymmetricAngleRad = .3;
inline constexpr double kSecondAsymmetricAngleRad = 1.8;
inline constexpr double kThirdAsymmetricAngleRad = 3.1;
inline constexpr double kFourthAsymmetricAngleRad = 4.9;
inline constexpr outshine::Vec3f kRoadColour{{.2f, .3f, .4f}};

struct GateSpec {
  double HeadingRad = 0;
  double HalfWidthM = 3;
};

outshine::RoadGate Gate(GateSpec spec) {
  const double east = std::cos(spec.HeadingRad);
  const double north = std::sin(spec.HeadingRad);
  return {.EastM = 8 * east,
          .NorthM = 8 * north,
          .GradeM = 5 + kSlopeE * 8 * east - kSlopeN * 8 * north,
          .OutE = east,
          .OutN = north,
          .HalfWidthM = spec.HalfWidthM};
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
    CHECK_NEAR(
        footprintArea, expectedArea, kAreaToleranceM2, "square metres", "complete gate footprint");
  }
  const Generators::ProfiledRoadMesher mesher;
  RoadMeshBuffers mesh;
  mesher.Junction(gates, {.SlopeE = kSlopeE, .SlopeN = -kSlopeN}, kRoadColour, mesh);
  double topArea = 0;
  for (size_t at = 0; at < mesh.PositionM.size(); at += 3) {
    if (mesh.NormalM[at + 1] <= 0) { continue; }
    const double height = 5 + kSlopeE * mesh.PositionM[at] + kSlopeN * mesh.PositionM[at + 2];
    CHECK_NEAR(
        mesh.PositionM[at + 1],
        height,
        kHeightToleranceM,
        "metres",
        "stored horizontal positions determine the shared plane without millimetre height steps");
  }
  for (size_t at = 0; at < mesh.Index.size(); at += 3) {
    const size_t a = static_cast<size_t>(mesh.Index[at]) * 3;
    const size_t b = static_cast<size_t>(mesh.Index[at + 1]) * 3;
    const size_t c = static_cast<size_t>(mesh.Index[at + 2]) * 3;
    std::array<double, 3> u{};
    std::array<double, 3> v{};
    std::array<double, 3> cross{};
    for (size_t axis = 0; axis < 3; ++axis) {
      u[axis] = static_cast<double>(mesh.PositionM[b + axis]) - mesh.PositionM[a + axis];
      v[axis] = static_cast<double>(mesh.PositionM[c + axis]) - mesh.PositionM[a + axis];
    }
    for (size_t axis = 0; axis < 3; ++axis) {
      cross[axis] = u[(axis + 1) % 3] * v[(axis + 2) % 3] - u[(axis + 2) % 3] * v[(axis + 1) % 3];
    }
    double facing = 0;
    double length = 0;
    for (size_t axis = 0; axis < 3; ++axis) {
      facing += cross[axis] * mesh.NormalM[a + axis];
      length += cross[axis] * cross[axis];
    }
    CHECK(length > kSquaredAreaFloorM4 && facing > 0,
          "every junction face is nondegenerate and outward");
    if (mesh.NormalM[a + 1] > 0) { topArea += std::abs(cross[1]) / 2; }
  }
  CHECK_NEAR(topArea,
             footprintArea,
             kCoverageToleranceM2,
             "square metres",
             "pavement covers the entire bed");
  for (const auto &corner : footprint.Rim) {
    bool found = false;
    for (size_t at = 0; at < mesh.PositionM.size(); at += 3) {
      found |= std::abs(mesh.PositionM[at] - corner.EastM) < kCornerToleranceM &&
               std::abs(mesh.PositionM[at + 2] + corner.NorthM) < kCornerToleranceM &&
               std::abs(mesh.PositionM[at + 1] -
                        (5 + kSlopeE * corner.EastM - kSlopeN * corner.NorthM)) < kCornerToleranceM;
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
    const std::array gates{Gate({.HeadingRad = 0}), Gate({.HeadingRad = pi - bend})};
    CheckJunction(gates, bend == 0 ? kStraightAreaM2 : 0);
    const std::array reversed{gates[1], gates[0]};
    CheckJunction(reversed);
  }
  CheckJunction(
      std::array{Gate({.HeadingRad = 0}), Gate({.HeadingRad = pi / 2}), Gate({.HeadingRad = pi})},
      kTeeAreaM2);
  CheckJunction(std::array{Gate({.HeadingRad = 0}),
                           Gate({.HeadingRad = pi / 2}),
                           Gate({.HeadingRad = pi}),
                           Gate({.HeadingRad = 3 * pi / 2})},
                kCrossingAreaM2);
  CheckJunction(std::array{Gate({.HeadingRad = kFirstAsymmetricAngleRad, .HalfWidthM = 2}),
                           Gate({.HeadingRad = kSecondAsymmetricAngleRad, .HalfWidthM = 4}),
                           Gate({.HeadingRad = kThirdAsymmetricAngleRad, .HalfWidthM = 3}),
                           Gate({.HeadingRad = kFourthAsymmetricAngleRad, .HalfWidthM = 2})});
  CHECK(Generators::BuildJunctionFootprint({}).Rim.empty(), "no gates produce no footprint");
  return Report();
}
