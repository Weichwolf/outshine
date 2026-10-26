#include "RoadCrossSection.h"
#include "math/RenderFrame.h"

#include <array>
#include <cmath>
#include <cstddef>
#include <span>

namespace outshine::Generators {

namespace {

constexpr double kShoulderFraction = 0.35;
constexpr double kTrackDepthM = 0.25;
constexpr double kShoulderDipM = 0.10;
constexpr double kKerbAcrossM = 0.35;

}

Section RoadSection(double halfWidthM, RoadProfile profile) {
  switch (profile) {
    case RoadProfile::Rounded:
      return {.HalfWidthM = halfWidthM,
              .ShoulderM = halfWidthM * kShoulderFraction,
              .ThicknessM = kTrackDepthM};
    case RoadProfile::Simple:
      return {.HalfWidthM = halfWidthM, .ShoulderM = kShoulderDipM, .ThicknessM = kSealedDepthM};
    case RoadProfile::Kerbed:
      return {.HalfWidthM = halfWidthM, .ShoulderM = kKerbAcrossM, .ThicknessM = kSealedDepthM};
  }
  return {};
}

RoadGate RoadEndGate(std::span<const RoadStation> stations, bool atEnd, Section section) {
  const auto &here = atEnd ? stations.back() : stations.front();
  const auto &adjacent = atEnd ? stations[stations.size() - 2] : stations[1];
  const double east = adjacent.EastM - here.EastM;
  const double north = adjacent.NorthM - here.NorthM;
  const double run = std::hypot(east, north);
  return {.EastM = here.EastM,
          .NorthM = here.NorthM,
          .GradeM = here.GradeM,
          .OutE = run > 0 ? east / run : 0,
          .OutN = run > 0 ? north / run : 0,
          .HalfWidthM = section.HalfWidthM,
          .ShoulderM = section.ShoulderM,
          .ThicknessM = section.ThicknessM};
}

std::array<Vec3f, kRibbonAcross> RoadPortPositions(const RoadPort &port) {
  const RoadGate &gate = port.Gate;
  const double width = gate.HalfWidthM;
  const std::array across{-width - gate.ShoulderM, -width, width, width + gate.ShoulderM};
  std::array<Vec3f, kRibbonAcross> positions;
  for (size_t at = 0; at < across.size(); ++at) {
    const auto east = static_cast<float>(gate.EastM - gate.OutN * across[at]);
    const auto north = static_cast<float>(gate.NorthM + gate.OutE * across[at]);
    const auto grade = static_cast<float>(gate.GradeM + port.Plane.SlopeE * (east - gate.EastM) +
                                          port.Plane.SlopeN * (north - gate.NorthM));
    positions[at] = {{east, grade, static_cast<float>(RenderFrame::ZOfNorth(north))}};
  }
  return positions;
}

Vec3f RoadPortNormal(RoadPlane plane) {
  const double length = std::hypot(plane.SlopeE, 1.0, plane.SlopeN);
  return {{static_cast<float>(-plane.SlopeE / length),
           static_cast<float>(1.0 / length),
           static_cast<float>(plane.SlopeN / length)}};
}

}
