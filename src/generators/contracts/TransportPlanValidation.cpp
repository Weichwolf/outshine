#include "TransportPlanFormat.h"
#include "TransportNetworkSnapshot.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <functional>
#include <limits>
#include <numbers>
#include <numeric>
#include <span>
#include <string>
#include <tuple>
#include <variant>

namespace outshine::Generators {
namespace {
template <class Range, class Before> bool StrictlyOrdered(const Range &items, Before before) {
  return std::adjacent_find(items.begin(), items.end(), [&](const auto &a, const auto &b) {
           return !before(a, b);
         }) == items.end();
}

bool ValidEnd(const TransportPlan &plan, const TransportPath &path, const TransportPathEnd &end) {
  if (const auto *boundary = std::get_if<TransportBoundaryPortReference>(&end)) {
    if (boundary->Port >= plan.Ports.size()) { return false; }
    const auto &port = plan.Ports[boundary->Port];
    return port.SourceId == path.SourceId && port.Layer == path.Layer && port.Use == path.Use &&
           port.Profile == path.Profile && port.Contact.Gate.HalfWidthM == path.HalfWidthM &&
           port.CrossfallRad == path.CrossfallRad;
  }
  if (const auto *gate = std::get_if<TransportJunctionGateReference>(&end)) {
    if (gate->Junction >= plan.Junctions.size()) { return false; }
    const auto &junction = plan.Junctions[gate->Junction];
    return gate->Gate < junction.Gates.size() &&
           junction.Gates[gate->Gate].HalfWidthM == path.HalfWidthM &&
           std::ranges::binary_search(junction.Sources, path.SourceId);
  }
  return std::holds_alternative<std::monostate>(end);
}

bool ProfileFits(const TransportPlan &plan, const TransportPath &path) {
  const auto stations = std::span(plan.Stations).subspan(path.FirstStation, path.StationCount);
  bool spans = false;
  for (size_t at = 1; at < stations.size(); ++at) {
    const auto &a = stations[at - 1];
    const auto &b = stations[at];
    const double length = std::hypot(b.EastM - a.EastM, b.NorthM - a.NorthM);
    const double change = std::abs(b.GradeM - a.GradeM);
    const double scale = std::max({1.0, std::abs(a.GradeM), std::abs(b.GradeM), length});
    constexpr double roundingOperations = 64;
    const double tolerance = roundingOperations * std::numeric_limits<double>::epsilon() * scale;
    if (!std::isfinite(length) || !std::isfinite(change) ||
        !std::isfinite(path.MaximumGradient * length) ||
        (!path.Body && change > path.MaximumGradient * length + tolerance)) {
      return false;
    }
    spans = spans || length > 0;
  }
  return spans;
}

bool PathsPartitionStations(const TransportPlan &plan) {
  size_t next = 0;
  for (const auto &path : plan.Paths) {
    if (path.FirstStation != next || path.StationCount > plan.Stations.size() - next) {
      return false;
    }
    next += path.StationCount;
  }
  return next == plan.Stations.size();
}

bool ValidPath(const TransportPlan &plan, const TransportPath &path) {
  return !path.PartId.empty() && !path.SourceId.empty() && !path.Material.empty() &&
         path.FirstStation <= plan.Stations.size() && path.StationCount >= 2 &&
         path.StationCount <= plan.Stations.size() - path.FirstStation &&
         std::isfinite(path.HalfWidthM) && path.HalfWidthM > 0 &&
         std::isfinite(path.CrossfallRad) && std::abs(path.CrossfallRad) < std::numbers::pi / 2 &&
         std::isfinite(path.MaximumGradient) && path.MaximumGradient >= 0 &&
         std::isfinite(path.SpeedMps) && path.SpeedMps >= 0 && path.Lanes >= 0 &&
         TransportFormat::ValidProfile(path.Profile) && TransportFormat::ValidUse(path.Use) &&
         (!path.Body || *path.Body < plan.Bodies.size()) && ValidEnd(plan, path, path.Ends[0]) &&
         ValidEnd(plan, path, path.Ends[1]) && ProfileFits(plan, path);
}

bool ValidJunction(const TransportJunction &junction) {
  return !junction.Sources.empty() && !junction.Material.empty() && !junction.Gates.empty() &&
         std::isfinite(junction.Plane.SlopeE) && std::isfinite(junction.Plane.SlopeN) &&
         StrictlyOrdered(junction.Sources, std::less<>{}) &&
         std::ranges::all_of(junction.Sources,
                             [](const auto &source) { return !source.empty(); }) &&
         std::ranges::all_of(junction.Gates, TransportFormat::ValidGate);
}

bool PortsOrdered(const TransportPlan &plan) {
  const auto key = [](const auto &port) {
    return std::tie(port.Boundary.Level,
                    port.Boundary.Axis,
                    port.Boundary.Line,
                    port.Boundary.Segment,
                    port.SourceId,
                    port.Anchor.LongitudeDeg,
                    port.Anchor.LatitudeDeg);
  };
  return StrictlyOrdered(plan.Ports, [&](const auto &a, const auto &b) { return key(a) < key(b); });
}
}

bool TransportFormat::InputsValid(std::span<const TransportInputReference> inputs) {
  return !inputs.empty() && std::ranges::all_of(inputs, [](const auto &input) {
    return !input.DatasetId.empty() && !input.ItemId.empty();
  }) && StrictlyOrdered(inputs, [](const auto &a, const auto &b) {
    return std::tie(a.DatasetId, a.Version, a.Revision, a.ItemId) <
           std::tie(b.DatasetId, b.Version, b.Revision, b.ItemId);
  });
}

std::expected<void, TransportPlanCodecError> ValidateTransportPlan(const TransportPlan &plan) {
  const auto &definition = plan.Definition;
  const auto bounds = definition.Core.Bounds();
  if (!bounds || definition.Representation != TransportRepresentation::Functional ||
      definition.RepresentationVersion != 2 ||
      plan.Anchor.LongitudeDeg != std::midpoint(bounds->WestDeg, bounds->EastDeg) ||
      plan.Anchor.LatitudeDeg != std::midpoint(bounds->SouthDeg, bounds->NorthDeg)) {
    return std::unexpected(TransportPlanCodecError::InvalidAddress);
  }
  if (plan.Coverage != TransportCoverage::Partial && plan.Coverage != TransportCoverage::Complete) {
    return std::unexpected(TransportPlanCodecError::UnknownCoverage);
  }
  if ((plan.Coverage == TransportCoverage::Complete) != plan.Omissions.empty() ||
      !StrictlyOrdered(plan.Omissions,
                       [](const auto &a, const auto &b) { return a.SourceId < b.SourceId; }) ||
      !std::ranges::all_of(plan.Omissions, [](const auto &omission) {
        return !omission.SourceId.empty() &&
               omission.Reason <= TransportOmissionReason::ResourceDeferred;
      })) {
    return std::unexpected(TransportPlanCodecError::InvalidOmission);
  }
  if ((plan.Coverage == TransportCoverage::Complete &&
       (!plan.Network || plan.Network->Coverage().empty())) ||
      (plan.Network && (!World::ValidateTransportNetwork(*plan.Network) ||
                        plan.Network->SourceIdentity().DatasetId.empty() ||
                        plan.Network->SourceIdentity().Revision.empty()))) {
    return std::unexpected(TransportPlanCodecError::InvalidNetwork);
  }
  if (!TransportFormat::InputsValid(plan.Inputs)) {
    return std::unexpected(TransportPlanCodecError::InvalidInputs);
  }
  if (!PathsPartitionStations(plan) ||
      !std::ranges::all_of(plan.Stations,
                           [](const auto &station) {
                             return std::isfinite(station.EastM) && std::isfinite(station.NorthM) &&
                                    std::isfinite(station.GradeM);
                           }) ||
      !StrictlyOrdered(plan.Paths,
                       [](const auto &a, const auto &b) { return a.PartId < b.PartId; }) ||
      !std::ranges::all_of(plan.Paths, [&](const auto &path) { return ValidPath(plan, path); })) {
    return std::unexpected(TransportPlanCodecError::InvalidPath);
  }
  if (!StrictlyOrdered(plan.Junctions,
                       [](const auto &a, const auto &b) { return a.Node < b.Node; }) ||
      !std::ranges::all_of(plan.Junctions, ValidJunction)) {
    return std::unexpected(TransportPlanCodecError::InvalidJunction);
  }
  if (!std::ranges::all_of(
          plan.Ports,
          [&](const auto &port) { return TransportFormat::ValidPort(port, definition.Core); }) ||
      !PortsOrdered(plan)) {
    return std::unexpected(TransportPlanCodecError::InvalidPort);
  }
  if (!std::ranges::all_of(plan.Bodies, TransportFormat::ValidBody)) {
    return std::unexpected(TransportPlanCodecError::InvalidBody);
  }
  if (!std::ranges::all_of(plan.TerrainContacts, TransportFormat::ValidContact)) {
    return std::unexpected(TransportPlanCodecError::InvalidContact);
  }
  return {};
}
}
