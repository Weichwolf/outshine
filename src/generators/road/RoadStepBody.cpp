#include "RoadStepBody.h"
#include "RoadCrossSection.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <limits>
#include <span>
#include <utility>
#include <vector>

namespace outshine::Generators {
namespace {
constexpr double kMaximumAxisDeviationM = .05;
constexpr double kMaximumLandingGrade = .125;
constexpr double kMinimumLandingM = .6;
constexpr double kMinimumTreadM = .24;

struct Input {
  SurfaceHeightTopology Topology;
  std::array<SurfaceHeightRecipe, 3> Sources;
  std::array<SurfaceHeightJunction, 2> Ports;
  SurfaceHeightFlight Flight;
  std::vector<double> Heights;
};

bool Finite(const RoadStation &station) {
  return std::isfinite(station.EastM) && std::isfinite(station.NorthM) &&
         std::isfinite(station.GradeM);
}

bool Aligned(std::span<const RoadStation> stations, PlanarPoint tangent, double length) {
  double previous = 0;
  for (const auto &station : stations) {
    if (!Finite(station)) { return false; }
    const double east = station.EastM - stations.front().EastM;
    const double north = station.NorthM - stations.front().NorthM;
    const double along = east * tangent[0] + north * tangent[1];
    if (along < previous || along > length + kMaximumAxisDeviationM ||
        std::abs(east * -tangent[1] + north * tangent[0]) > kMaximumAxisDeviationM) {
      return false;
    }
    previous = along;
  }
  return true;
}

using Profile = std::array<SurfaceHeightPoint, kRibbonAcross>;

bool Fits(const RoadPort &port,
          const RoadStation &station,
          PlanarPoint tangent,
          Section section,
          double direction) {
  const auto &gate = port.Gate;
  constexpr double minimumAlignment = .99;
  constexpr double unitTolerance = 1e-10;
  return std::hypot(gate.EastM - station.EastM, gate.NorthM - station.NorthM) <=
             kMaximumAxisDeviationM &&
         gate.HalfWidthM == section.HalfWidthM && gate.ShoulderM == section.ShoulderM &&
         gate.ThicknessM == section.ThicknessM &&
         std::abs(std::hypot(gate.OutE, gate.OutN) - 1) <= unitTolerance &&
         direction * (gate.OutE * tangent[0] + gate.OutN * tangent[1]) >= minimumAlignment;
}

std::expected<Profile, RoadStepError> Boundary(const RoadPort &port, bool reverse) {
  const auto points = RoadPortPositions(port);
  Profile profile;
  for (size_t at = 0; at < points.size(); ++at) {
    const auto point = points[reverse ? points.size() - 1 - at : at];
    profile[at] = {point[0], -static_cast<double>(point[2]), point[1]};
    if (!std::ranges::all_of(profile[at], [](double value) { return std::isfinite(value); })) {
      return std::unexpected(RoadStepError::InvalidInput);
    }
  }
  return profile;
}

void AppendProfile(Input &input, const Profile &profile) {
  for (const auto point : profile) {
    input.Topology.Points.push_back({point[0], point[1]});
    input.Heights.push_back(point[2]);
  }
}

void Faces(Input &input) {
  input.Topology.Sources.resize(3);
  for (uint32_t row = 0; row < 3; ++row) {
    for (uint32_t across = 0; across < kRibbonAcross - 1; ++across) {
      const uint32_t first = row * kRibbonAcross + across;
      const uint32_t next = first + kRibbonAcross;
      for (auto face :
           {PlanarFace{first, next, first + 1}, PlanarFace{first + 1, next, next + 1}}) {
        const auto a = input.Topology.Points[face[0]];
        const auto b = input.Topology.Points[face[1]];
        const auto c = input.Topology.Points[face[2]];
        if ((b[0] - a[0]) * (c[1] - a[1]) - (b[1] - a[1]) * (c[0] - a[0]) < 0) {
          std::swap(face[1], face[2]);
        }
        input.Topology.Sources[row].Faces.push_back(face);
      }
    }
  }
}

std::expected<Input, RoadStepError> Prepare(std::span<const RoadStation> stations,
                                            const RoadSweep &surface) {
  if (stations.size() < 2 || stations.size() > SurfaceBodyBudget{}.Points ||
      !std::ranges::all_of(stations, Finite) || !std::isfinite(surface.HalfWidthM) ||
      surface.HalfWidthM <= 0 || surface.Profile != RoadProfile::Simple) {
    return std::unexpected(RoadStepError::InvalidInput);
  }
  const auto section = RoadSection(surface.HalfWidthM, surface.Profile);
  const auto start = surface.EndPorts[0].value_or(
      RoadPort{.Gate = RoadEndGate(stations, false, section), .Plane = {}});
  const auto end = surface.EndPorts[1].value_or(
      RoadPort{.Gate = RoadEndGate(stations, true, section), .Plane = {}});
  const auto first = Boundary(start, false);
  const auto last = Boundary(end, true);
  if (!first || !last) { return std::unexpected(RoadStepError::InvalidInput); }
  const double east = end.Gate.EastM - start.Gate.EastM;
  const double north = end.Gate.NorthM - start.Gate.NorthM;
  const double length = std::hypot(east, north);
  const PlanarPoint tangent{east / length, north / length};
  if (!std::isfinite(length) || length <= 0 || !Aligned(stations, tangent, length) ||
      !Fits(start, stations.front(), tangent, section, 1) ||
      !Fits(end, stations.back(), tangent, section, -1)) {
    return std::unexpected(RoadStepError::UnsupportedGeometry);
  }
  std::array<double, 2> landings{std::max(kMinimumLandingM, surface.HalfWidthM),
                                 std::max(kMinimumLandingM, surface.HalfWidthM)};
  for (size_t at = 0; at < kRibbonAcross; ++at) {
    landings[0] =
        std::max(landings[0], std::abs((*first)[at][2] - start.Gate.GradeM) / kMaximumLandingGrade);
    landings[1] =
        std::max(landings[1], std::abs((*last)[at][2] - end.Gate.GradeM) / kMaximumLandingGrade);
  }
  const double rise = end.Gate.GradeM - start.Gate.GradeM;
  const double risers = std::ceil(rise / SurfaceBodyBudget::kDefaultMaximumRiseM);
  if (risers < 1 || risers > static_cast<double>(SurfaceBodyBudget{}.Risers) ||
      length - landings[0] - landings[1] < risers * kMinimumTreadM) {
    return std::unexpected(RoadStepError::UnsupportedGeometry);
  }
  Input input;
  AppendProfile(input, *first);
  const std::array across{-section.HalfWidthM - section.ShoulderM,
                          -section.HalfWidthM,
                          section.HalfWidthM,
                          section.HalfWidthM + section.ShoulderM};
  const std::array stationsM{landings[0], length - landings[1]};
  for (size_t endAt = 0; endAt < stationsM.size(); ++endAt) {
    Profile profile;
    const PlanarPoint origin{start.Gate.EastM + tangent[0] * stationsM[endAt],
                             start.Gate.NorthM + tangent[1] * stationsM[endAt]};
    input.Ports[endAt].Origin = origin;
    for (size_t at = 0; at < profile.size(); ++at) {
      profile[at] = {origin[0] - tangent[1] * across[at],
                     origin[1] + tangent[0] * across[at],
                     endAt == 0 ? start.Gate.GradeM : end.Gate.GradeM};
    }
    AppendProfile(input, profile);
  }
  AppendProfile(input, *last);
  Faces(input);
  input.Sources[1].Steps = true;
  input.Sources[1].Axis = {{start.Gate.EastM, start.Gate.NorthM},
                           {end.Gate.EastM, end.Gate.NorthM}};
  input.Flight = {.Source = 1,
                  .Start = 0,
                  .End = 1,
                  .Direction = 1,
                  .CutStart = stationsM[0],
                  .CutEnd = stationsM[1],
                  .Stations = {}};
  return input;
}
}

std::expected<SurfaceBody, RoadStepError> BuildRoadStepBody(std::span<const RoadStation> stations,
                                                            const RoadSweep &surface) {
  if (stations.size() >= 2 && stations.front().GradeM > stations.back().GradeM) {
    std::vector<RoadStation> reversed(stations.rbegin(), stations.rend());
    auto reverseSurface = surface;
    std::swap(reverseSurface.EndPorts[0], reverseSurface.EndPorts[1]);
    return BuildRoadStepBody(reversed, reverseSurface);
  }
  auto input = Prepare(stations, surface);
  if (!input) { return std::unexpected(input.error()); }
  const SurfaceHeightRequest request{.Topology = input->Topology,
                                     .Sources = input->Sources,
                                     .Ports = input->Ports,
                                     .Flights = std::span(&input->Flight, 1),
                                     .Contacts = {},
                                     .Orders = {},
                                     .MaximumAdjustmentM = 0,
                                     .MaximumCrossfall = 0};
  auto body = BuildSurfaceBody(request, input->Heights);
  if (!body) { return std::unexpected(RoadStepError::BodyFailure); }
  return std::move(*body);
}
}
