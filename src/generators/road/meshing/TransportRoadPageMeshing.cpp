#include "TransportRoadPageMeshing.h"
#include "RoadPageMeshing.h"
#include "TransportPortFrame.h"
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <expected>
#include <optional>
#include <span>
#include <stop_token>
#include <string_view>
#include <variant>

namespace outshine::Generators {
namespace {
bool Before(const RoadPagePartAppearance &a, const RoadPagePartAppearance &b) {
  return a.Part.Kind != b.Part.Kind ? a.Part.Kind < b.Part.Kind : a.Part.Index < b.Part.Index;
}

std::expected<EastNorth, std::string_view> InteriorPoint(std::span<const RoadStation> stations,
                                                         size_t end) {
  const auto &endpoint = end == 0 ? stations.front() : stations.back();
  for (size_t step = 1; step < stations.size(); ++step) {
    const auto &candidate = stations[end == 0 ? step : stations.size() - 1 - step];
    if (candidate.EastM != endpoint.EastM || candidate.NorthM != endpoint.NorthM) {
      return EastNorth{.EastM = candidate.EastM, .NorthM = candidate.NorthM};
    }
  }
  return std::unexpected("road page boundary path has no interior direction");
}

std::expected<std::optional<RoadPort>, std::string_view>
EndPort(const TransportPlan &plan,
        const TangentFrame &frame,
        const TransportPathEnd &reference,
        std::span<const RoadStation> stations,
        size_t end) {
  if (const auto *boundary = std::get_if<TransportBoundaryPortReference>(&reference)) {
    if (boundary->Port >= plan.Ports.size()) {
      return std::unexpected("road page path refers to a missing boundary contact");
    }
    const auto interior = InteriorPoint(stations, end);
    if (!interior) { return std::unexpected(interior.error()); }
    const auto contact = TransportPortInFrame(plan.Ports[boundary->Port], frame, *interior);
    if (!contact) { return std::unexpected(contact.error()); }
    return std::optional<RoadPort>(*contact);
  }
  if (const auto *gate = std::get_if<TransportJunctionGateReference>(&reference)) {
    if (gate->Junction >= plan.Junctions.size() ||
        gate->Gate >= plan.Junctions[gate->Junction].Gates.size()) {
      return std::unexpected("road page path refers to a missing junction contact");
    }
    const auto &junction = plan.Junctions[gate->Junction];
    return std::optional<RoadPort>(
        RoadPort{.Gate = junction.Gates[gate->Gate], .Plane = junction.Plane});
  }
  return std::nullopt;
}

std::expected<void, std::string_view> Interval(const RoadMesher &mesher,
                                               const TransportPlan &plan,
                                               const TangentFrame &frame,
                                               const RoadPagePartAppearance &selected,
                                               RoadPageGeometry &product) {
  if (selected.Part.Index >= plan.Paths.size()) {
    return std::unexpected("road page requests an unknown transport path");
  }
  const auto &path = plan.Paths[selected.Part.Index];
  if (path.FirstStation > plan.Stations.size() || path.StationCount < 2 ||
      path.StationCount > plan.Stations.size() - path.FirstStation ||
      (path.Body && *path.Body >= plan.Bodies.size())) {
    return std::unexpected("road page path has no complete native geometry");
  }
  const auto stations = std::span(plan.Stations).subspan(path.FirstStation, path.StationCount);
  RoadSweep surface{.HalfWidthM = path.HalfWidthM,
                    .Profile = path.Profile,
                    .WearsLinear = selected.WearsLinear,
                    .Crossfall = path.CrossfallRad,
                    .Form = path.Bridge ? RibbonForm::ClosedShell : RibbonForm::Surface};
  if (!path.Body) {
    for (size_t end = 0; end < path.Ends.size(); ++end) {
      const auto contact = EndPort(plan, frame, path.Ends[end], stations, end);
      if (!contact) { return std::unexpected(contact.error()); }
      surface.EndPorts[end] = *contact;
    }
  }
  return AppendRoadPageInterval(mesher,
                                {.Edge = selected.Part.Index,
                                 .Source = selected.Part.Index,
                                 .Stations = stations,
                                 .Surface = surface,
                                 .Body = path.Body ? &plan.Bodies[*path.Body] : nullptr},
                                product);
}
}

std::expected<RoadPageGeometry, std::string_view>
MeshTransportRoadPage(const RoadMesher &mesher,
                      const TransportPlan &plan,
                      std::span<const RoadPagePartAppearance> parts,
                      const std::stop_token &stop) {
  if (plan.Coverage != TransportCoverage::Complete || !plan.Definition.Core.Valid()) {
    return std::unexpected("road page requires a complete functional transport plan");
  }
  if (std::ranges::adjacent_find(
          parts, [](const auto &a, const auto &b) { return !Before(a, b); }) != parts.end()) {
    return std::unexpected("road page selection is duplicated or unordered");
  }
  RoadPageGeometry product;
  const auto frame = TangentFrame::At(plan.Anchor);
  for (const auto &selected : parts) {
    if (stop.stop_requested()) { return std::unexpected("road page generation canceled"); }
    if (!std::ranges::all_of(selected.WearsLinear.Axis,
                             [](float value) { return std::isfinite(value) && value >= 0; })) {
      return std::unexpected("road page material binding is not finite and nonnegative");
    }
    if (selected.Part.Kind == RoadPagePartKind::Interval) {
      const auto appended = Interval(mesher, plan, frame, selected, product);
      if (!appended) { return std::unexpected(appended.error()); }
    } else if (selected.Part.Kind == RoadPagePartKind::Junction &&
               selected.Part.Index < plan.Junctions.size()) {
      const auto &junction = plan.Junctions[selected.Part.Index];
      AppendRoadPageJunction(mesher,
                             {.Index = selected.Part.Index,
                              .Node = junction.Node,
                              .Gates = junction.Gates,
                              .Plane = junction.Plane,
                              .WearsLinear = selected.WearsLinear},
                             product);
    } else {
      return std::unexpected("road page requests an unknown transport junction");
    }
    if (!product.Diagnostics.empty()) {
      return std::unexpected("road page cannot fulfill its selected complete transport geometry");
    }
  }
  if (stop.stop_requested()) { return std::unexpected("road page generation canceled"); }
  return product;
}
}
