#include "TransportPlanFormat.h"
#include "Units.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <numbers>
#include <span>

namespace outshine::Generators::TransportFormat {
namespace {
template <size_t N> bool Finite(const std::array<double, N> &values) {
  return std::ranges::all_of(values, [](double value) { return std::isfinite(value); });
}

bool Ring(std::span<const double> values, bool empty) {
  return ((empty && values.empty()) || (values.size() >= 6 && values.size() % 2 == 0)) &&
         std::ranges::all_of(values, [](double value) { return std::isfinite(value); });
}

bool OnBoundary(const TransportPort &port) {
  const auto &edge = port.Boundary;
  const auto count = static_cast<double>(uint32_t{1} << static_cast<unsigned>(edge.Level));
  const double longitude =
      port.Anchor.LongitudeDeg == kDegPerHalfTurn ? -kDegPerHalfTurn : port.Anchor.LongitudeDeg;
  if (edge.Axis == TransportBoundaryAxis::Longitude) {
    return longitude == -kDegPerHalfTurn + kDegPerTurn * static_cast<double>(edge.Line) / count &&
           port.Anchor.LatitudeDeg >=
               -kDegPerHalfTurn / 2 + kDegPerHalfTurn * static_cast<double>(edge.Segment) / count &&
           port.Anchor.LatitudeDeg <=
               -kDegPerHalfTurn / 2 +
                   kDegPerHalfTurn * static_cast<double>(edge.Segment + 1) / count;
  }
  return port.Anchor.LatitudeDeg ==
             -kDegPerHalfTurn / 2 + kDegPerHalfTurn * static_cast<double>(edge.Line) / count &&
         port.Anchor.LongitudeDeg >=
             -kDegPerHalfTurn + kDegPerTurn * static_cast<double>(edge.Segment) / count &&
         port.Anchor.LongitudeDeg <=
             -kDegPerHalfTurn + kDegPerTurn * static_cast<double>(edge.Segment + 1) / count;
}

bool Profile(const ProfiledCorridorSpan &profile) {
  return Finite(std::array{profile.BeginM.EastM,
                           profile.BeginM.NorthM,
                           profile.EndM.EastM,
                           profile.EndM.NorthM,
                           profile.BeginDerivativeM.EastM,
                           profile.BeginDerivativeM.NorthM,
                           profile.BeginDerivativeM.UpM,
                           profile.EndDerivativeM.EastM,
                           profile.EndDerivativeM.NorthM,
                           profile.EndDerivativeM.UpM,
                           profile.StationLengthM,
                           profile.BeginBedM,
                           profile.EndBedM,
                           profile.BeginPavementHalfWidthM,
                           profile.EndPavementHalfWidthM,
                           profile.BeginHalfWidthM,
                           profile.EndHalfWidthM}) &&
         profile.StationLengthM > 0 && profile.BeginPavementHalfWidthM >= 0 &&
         profile.EndPavementHalfWidthM >= 0 &&
         profile.BeginHalfWidthM >= profile.BeginPavementHalfWidthM &&
         profile.EndHalfWidthM >= profile.EndPavementHalfWidthM;
}
}

bool ValidProfile(RoadProfile profile) {
  return profile == RoadProfile::Rounded || profile == RoadProfile::Simple ||
         profile == RoadProfile::Kerbed;
}

bool ValidUse(TransportUse use) {
  return use == TransportUse::Road || use == TransportUse::Path || use == TransportUse::Rail;
}

bool ValidGate(const RoadGate &gate) {
  constexpr double directionTolerance = 1e-10;
  return Finite(std::array{gate.EastM,
                           gate.NorthM,
                           gate.GradeM,
                           gate.OutE,
                           gate.OutN,
                           gate.HalfWidthM,
                           gate.ShoulderM,
                           gate.ThicknessM}) &&
         gate.HalfWidthM > 0 && gate.ShoulderM >= 0 && gate.ThicknessM >= 0 &&
         std::abs(std::hypot(gate.OutE, gate.OutN) - 1) <= directionTolerance;
}

bool ValidPort(const TransportPort &port, GeoCellId core) {
  if (!port.Boundary.Valid() || port.SourceId.empty() || !ValidGate(port.Contact.Gate) ||
      port.Contact.Gate.EastM != 0 || port.Contact.Gate.NorthM != 0 ||
      !Finite(std::array{port.Anchor.LongitudeDeg,
                         port.Anchor.LatitudeDeg,
                         port.Contact.Plane.SlopeE,
                         port.Contact.Plane.SlopeN,
                         port.CrossfallRad}) ||
      std::abs(port.Anchor.LongitudeDeg) > kDegPerHalfTurn ||
      std::abs(port.Anchor.LatitudeDeg) > kDegPerHalfTurn / 2 ||
      std::abs(port.CrossfallRad) >= std::numbers::pi / 2 || !ValidProfile(port.Profile) ||
      !ValidUse(port.Use) || !OnBoundary(port)) {
    return false;
  }
  return std::ranges::any_of(std::array{TransportCoreSide::West,
                                        TransportCoreSide::East,
                                        TransportCoreSide::South,
                                        TransportCoreSide::North},
                             [&](auto side) {
                               const auto edge = TransportBoundaryOf(core, side);
                               return edge && *edge == port.Boundary;
                             });
}

bool ValidBody(const HeightSolid &body) {
  return !body.Points.empty() && !body.Faces.empty() && body.SurfaceFaces > 0 &&
         body.SurfaceFaces <= body.Faces.size() && std::isfinite(body.VolumeM3) &&
         body.VolumeM3 >= 0 && body.WallParents.size() == body.WallKinds.size() &&
         std::ranges::all_of(body.Points, [](const auto &point) { return Finite(point); }) &&
         std::ranges::all_of(body.Faces,
                             [&](const auto &face) {
                               return face[0] != face[1] && face[1] != face[2] &&
                                      face[2] != face[0] &&
                                      std::ranges::all_of(face, [&](uint32_t index) {
                                        return index < body.Points.size();
                                      });
                             }) &&
         std::ranges::all_of(body.WallParents,
                             [&](uint32_t parent) { return parent < body.SurfaceFaces; }) &&
         std::ranges::all_of(body.WallKinds, [](auto kind) {
           return kind == HeightSolidWallKind::Boundary ||
                  kind == HeightSolidWallKind::Discontinuity;
         });
}

bool ValidContact(const EarthworkStamp &stamp) {
  return Ring(stamp.RingEastNorthM, true) &&
         std::ranges::all_of(stamp.HoleRingsEastNorthM,
                             [](const auto &hole) { return Ring(hole, false); }) &&
         stamp.SeamEastNorthM.size() % 2 == 0 &&
         std::ranges::all_of(stamp.SeamEastNorthM,
                             [](double value) { return std::isfinite(value); }) &&
         Finite(std::array{stamp.LowE,
                           stamp.HighE,
                           stamp.LowN,
                           stamp.HighN,
                           stamp.AtE,
                           stamp.AtN,
                           stamp.PlateauM,
                           stamp.SlopeE,
                           stamp.SlopeN,
                           stamp.ApronM,
                           stamp.YieldM,
                           stamp.SagInv}) &&
         stamp.LowE <= stamp.HighE && stamp.LowN <= stamp.HighN && stamp.ApronM >= 0 &&
         (!stamp.Profile || Profile(*stamp.Profile)) &&
         (!stamp.PlanarSupportM || std::isfinite(*stamp.PlanarSupportM)) &&
         (stamp.Kind == EarthworkKind::Pad || stamp.Kind == EarthworkKind::Corridor ||
          stamp.Kind == EarthworkKind::Basin || stamp.Kind == EarthworkKind::Clearance);
}
}
