#ifndef OUTSHINE_TEST_TRANSPORTPLANFIXTURE_H
#define OUTSHINE_TEST_TRANSPORTPLANFIXTURE_H

#include "TransportPlanCodec.h"
#include "TransportNetworkFixture.h"
#include "TangentFrame.h"
#include <array>
#include <cstddef>
#include <cstdint>
#include <numeric>
#include <optional>
#include <utility>

namespace outshine::Test {
inline constexpr double kFixtureSlopeE = 0.001;
inline constexpr double kFixtureSlopeN = 0.002;
inline constexpr uint64_t kFixtureCorridor = 42;
inline constexpr double kFixtureLengthM = 20;
inline constexpr double kFixtureBeginBedM = 10.1;
inline constexpr double kFixtureEndBedM = 10.2;
inline constexpr double kFixtureGradient = 0.1;
inline constexpr double kFixtureYieldM = 0.1;
inline constexpr double kFixtureSagInv = 0.0001;
inline constexpr uint32_t kFixtureCoreX = 34000;
inline constexpr uint32_t kFixtureCoreY = 50000;
inline constexpr double kFixtureBoundaryGradeM = 10.01;
inline constexpr double kFixtureCrossfallRad = 0.025;
inline constexpr double kFixtureDeadEndM = 100;
inline constexpr double kFixtureSpeedMps = 13.9;

inline constexpr Generators::TransportPlanCodecLimits kTransportFixtureLimits{
    .PackageBytesMost = size_t{1024} * 1024, .DecodedCapacityMost = size_t{4} * 1024 * 1024};

inline std::optional<HeightSolid> TransportFixtureBody() {
  const std::array<PlanarPoint, 3> points{{{-1, -1}, {1, -1}, {0, 1}}};
  const std::array<PlanarFace, 1> faces{{{0, 1, 2}}};
  const std::array<std::array<double, 3>, 1> tops{{{12, 12, 12}}};
  const std::array<double, 3> bottoms{11, 11, 11};
  auto body = BuildHeightSolid(
      {.Points = points, .Faces = faces, .TopHeights = tops, .BottomHeights = bottoms});
  if (!body) { return std::nullopt; }
  return std::move(*body);
}

inline EarthworkStamp TransportFixtureContact() {
  EarthworkStamp stamp;
  stamp.RingEastNorthM = {-10, -5, 10, -5, 10, 5, -10, 5};
  stamp.HoleRingsEastNorthM = {{-1, -1, 1, -1, 1, 1, -1, 1}};
  stamp.LowE = -10;
  stamp.HighE = 10;
  stamp.LowN = -5;
  stamp.HighN = 5;
  stamp.PlateauM = 10;
  stamp.SlopeE = kFixtureSlopeE;
  stamp.SlopeN = kFixtureSlopeN;
  stamp.SeamEastNorthM = {-10, 0, 10, 0};
  stamp.Profile = ProfiledCorridorSpan{.CorridorKey = kFixtureCorridor,
                                       .BeginM = {.EastM = -10},
                                       .EndM = {.EastM = 10},
                                       .BeginDerivativeM = {.EastM = kFixtureLengthM},
                                       .EndDerivativeM = {.EastM = kFixtureLengthM},
                                       .StationLengthM = kFixtureLengthM,
                                       .BeginBedM = kFixtureBeginBedM,
                                       .EndBedM = kFixtureEndBedM,
                                       .BeginPavementHalfWidthM = 4,
                                       .EndPavementHalfWidthM = 4,
                                       .BeginHalfWidthM = 6,
                                       .EndHalfWidthM = 6};
  stamp.PlanarSupportM = 10;
  stamp.CorridorKey = kFixtureCorridor;
  stamp.ApronM = 2;
  stamp.YieldM = kFixtureYieldM;
  stamp.SagInv = kFixtureSagInv;
  stamp.Kind = EarthworkKind::Corridor;
  return stamp;
}

inline std::optional<Generators::TransportPlan> TransportPlanFixture() {
  using namespace Generators;
  TransportPlan plan(
      TransportPlanDefinition{.Core = {.Level = 16, .X = kFixtureCoreX, .Y = kFixtureCoreY}});
  const auto bounds = plan.Definition.Core.Bounds();
  const auto edge = TransportBoundaryOf(plan.Definition.Core, TransportCoreSide::East);
  if (!bounds || !edge) { return std::nullopt; }
  plan.Anchor = {.LongitudeDeg = std::midpoint(bounds->WestDeg, bounds->EastDeg),
                 .LatitudeDeg = std::midpoint(bounds->SouthDeg, bounds->NorthDeg)};
  plan.Coverage = TransportCoverage::Complete;
  plan.Network = TransportNetworkFixture(plan.Anchor, *bounds);
  TransportInputReference input;
  input.DatasetId = "fixture-source";
  input.Version = "v1";
  input.Revision = "snapshot-1";
  input.ItemId = "transport/42";
  for (size_t at = 0; at < input.Content.size(); ++at) {
    input.Content[at] = static_cast<uint8_t>(128 + at);
  }
  plan.Inputs.push_back(std::move(input));
  TransportPort port;
  port.Boundary = *edge;
  port.SourceId = "fixture/road/42";
  port.Inputs.fill(uint8_t{7});
  port.Anchor = {.LongitudeDeg = bounds->EastDeg, .LatitudeDeg = plan.Anchor.LatitudeDeg};
  port.Contact = {.Gate = {.GradeM = kFixtureBoundaryGradeM,
                           .OutE = 1,
                           .HalfWidthM = 4,
                           .ShoulderM = 0.5,
                           .ThicknessM = kSealedDepthM},
                  .Plane = {.SlopeE = kFixtureSlopeE, .SlopeN = kFixtureSlopeN}};
  port.Profile = RoadProfile::Kerbed;
  port.CrossfallRad = kFixtureCrossfallRad;
  plan.Ports.push_back(std::move(port));
  const auto position = TangentFrame::At(plan.Anchor).ToLocalGroundPosition(plan.Ports[0].Anchor);
  plan.Stations = {
      {.EastM = 1, .GradeM = 10, .Node = 7},
      {.EastM = position.EastM, .NorthM = position.NorthM, .GradeM = kFixtureBoundaryGradeM},
      {.EastM = -kFixtureDeadEndM, .GradeM = 10},
      {.EastM = -1, .GradeM = 10, .Node = 7}};
  TransportJunction junction;
  junction.Node = 7;
  junction.Sources = {"fixture/road/42"};
  junction.Material = "asphalt";
  junction.Gates = {{.EastM = 1, .GradeM = 10, .OutE = 1, .HalfWidthM = 4, .ShoulderM = 0.5},
                    {.EastM = -1, .GradeM = 10, .OutE = -1, .HalfWidthM = 4, .ShoulderM = 0.5}};
  junction.Plane = {.SlopeE = kFixtureSlopeE, .SlopeN = kFixtureSlopeN};
  plan.Junctions.push_back(std::move(junction));
  TransportPath path;
  path.PartId = "road/42/0";
  path.SourceId = "fixture/road/42";
  path.Material = "asphalt";
  path.StationCount = 2;
  path.HalfWidthM = 4;
  path.CrossfallRad = kFixtureCrossfallRad;
  path.MaximumGradient = kFixtureGradient;
  path.SpeedMps = kFixtureSpeedMps;
  path.Profile = RoadProfile::Kerbed;
  path.Lanes = 2;
  path.Ends = {TransportJunctionGateReference{.Junction = 0, .Gate = 0},
               TransportBoundaryPortReference{.Port = 0}};
  plan.Paths.push_back(path);
  path.PartId = "road/42/1";
  path.FirstStation = 2;
  path.Ends = {std::monostate{}, TransportJunctionGateReference{.Junction = 0, .Gate = 1}};
  plan.Paths.push_back(std::move(path));
  auto body = TransportFixtureBody();
  if (!body) { return std::nullopt; }
  plan.Bodies.push_back(std::move(*body));
  plan.TerrainContacts.push_back(TransportFixtureContact());
  const auto binding =
      TransportPlanInputBinding(plan.Inputs, kTransportFixtureLimits.PackageBytesMost);
  if (!binding) { return std::nullopt; }
  plan.InputBinding = *binding;
  return plan;
}
}
#endif
