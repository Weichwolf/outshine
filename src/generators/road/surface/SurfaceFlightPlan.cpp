#include "SurfaceFlightPlan.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <limits>
#include <span>
#include <stop_token>
#include <utility>
#include <vector>

namespace outshine::Generators {
namespace {
using Error = SurfaceFlightError;
template <typename T> using Checked = std::expected<T, Error>;

struct Planner {
  const SurfaceHeightTopology &Topology;
  std::span<const SurfaceHeightRecipe> Sources;
  SurfaceFlightBudget Budget;
  const std::stop_token &Stop;
  SurfaceFlightPlan Output;

  Checked<void> Visit(size_t added = 1) {
    if (Stop.stop_requested()) { return std::unexpected(Error::Canceled); }
    if (added > Budget.VertexVisits - Output.Work.VertexVisits) {
      return std::unexpected(Error::CapacityExceeded);
    }
    Output.Work.VertexVisits += added;
    return {};
  }
};

bool Finite(PlanarPoint point) {
  return std::isfinite(point[0]) && std::isfinite(point[1]);
}

Checked<PlanarStationAxis> FlightAxis(Planner &planner, uint32_t owner) {
  const auto &axis = planner.Sources[owner].Axis;
  if (axis.size() < 2) { return std::unexpected(Error::InvalidInput); }
  const auto origin = axis.front();
  PlanarPoint tangent{axis.back()[0] - origin[0], axis.back()[1] - origin[1]};
  const double length = std::sqrt(tangent[0] * tangent[0] + tangent[1] * tangent[1]);
  if (!std::isfinite(length) || length <= 0) { return std::unexpected(Error::InvalidInput); }
  for (auto &value : tangent) { value /= length; }
  for (const auto point : axis) {
    if (const auto visited = planner.Visit(); !visited) { return std::unexpected(visited.error()); }
    if (!Finite(point)) { return std::unexpected(Error::InvalidInput); }
    const double sideways =
        (point[0] - origin[0]) * -tangent[1] + (point[1] - origin[1]) * tangent[0];
    if (std::abs(sideways) > planner.Budget.StraightnessM) {
      return std::unexpected(Error::CurvedAxis);
    }
  }
  return PlanarStationAxis{.Origin = origin, .Direction = tangent, .Cuts = {}};
}

Checked<std::vector<bool>> SourceVertices(Planner &planner, uint32_t source) {
  std::vector<bool> present(planner.Topology.Points.size());
  for (const auto face : planner.Topology.Sources[source].Faces) {
    if (const auto visited = planner.Visit(face.size()); !visited) {
      return std::unexpected(visited.error());
    }
    for (const auto vertex : face) {
      if (vertex >= present.size()) { return std::unexpected(Error::InvalidInput); }
      present[vertex] = true;
    }
  }
  return present;
}

Checked<double> LandingStation(Planner &planner,
                               std::span<const double> stations,
                               const std::vector<bool> &source,
                               uint32_t port,
                               bool start) {
  double station =
      start ? -std::numeric_limits<double>::infinity() : std::numeric_limits<double>::infinity();
  bool found = false;
  for (const auto vertex : planner.Topology.PortVertices[port]) {
    if (const auto visited = planner.Visit(); !visited) { return std::unexpected(visited.error()); }
    if (vertex >= stations.size()) { return std::unexpected(Error::InvalidInput); }
    if (!source[vertex]) { continue; }
    found = true;
    station = start ? std::max(station, stations[vertex]) : std::min(station, stations[vertex]);
  }
  if (!found) { return std::unexpected(Error::MissingLanding); }
  return station;
}

Checked<double>
ClearCut(Planner &planner, std::span<const double> stations, double required, double direction) {
  const double other = required + direction * planner.Budget.LandingSearchM;
  const double lower = std::min(required, other);
  const double upper = std::max(required, other);
  if (!std::isfinite(other) || !(lower < upper)) { return std::unexpected(Error::NoSlopedRun); }
  std::vector<double> values{lower, upper};
  values.reserve(stations.size() + 2);
  for (const auto station : stations) {
    if (const auto visited = planner.Visit(); !visited) { return std::unexpected(visited.error()); }
    if (station > lower && station < upper) { values.push_back(station); }
  }
  std::ranges::sort(values);
  values.erase(std::ranges::unique(values).begin(), values.end());
  double largest = 0;
  double middle = lower;
  for (size_t at = 1; at < values.size(); ++at) {
    ++planner.Output.Work.GapCandidates;
    const double gap = values[at] - values[at - 1];
    if (gap > largest) {
      largest = gap;
      middle = (values[at - 1] + values[at]) / 2;
    }
  }
  if (!std::isfinite(middle) || middle <= lower || middle >= upper) {
    return std::unexpected(Error::NoSlopedRun);
  }
  return middle;
}

Checked<void> AppendFlight(Planner &planner, const SurfaceFlightEndpoints &ports) {
  const auto axis = FlightAxis(planner, ports.Source);
  if (!axis) { return std::unexpected(axis.error()); }
  const auto present = SourceVertices(planner, ports.Source);
  if (!present) { return std::unexpected(present.error()); }
  std::vector<double> stations;
  stations.reserve(planner.Topology.Points.size());
  for (const auto point : planner.Topology.Points) {
    if (const auto visited = planner.Visit(); !visited) { return std::unexpected(visited.error()); }
    const double along = (point[0] - axis->Origin[0]) * axis->Direction[0] +
                         (point[1] - axis->Origin[1]) * axis->Direction[1];
    if (!Finite(point) || !std::isfinite(along)) { return std::unexpected(Error::InvalidInput); }
    stations.push_back(along);
    ++planner.Output.Work.PointProjections;
  }
  const auto start = LandingStation(planner, stations, *present, ports.Start, true);
  const auto end = LandingStation(planner, stations, *present, ports.End, false);
  if (!start || !end) { return std::unexpected(!start ? start.error() : end.error()); }
  const auto first = ClearCut(planner, stations, *start, 1);
  const auto last = ClearCut(planner, stations, *end, -1);
  if (!first || !last) { return std::unexpected(!first ? first.error() : last.error()); }
  const auto &points = planner.Sources[ports.Source].Axis;
  const double length =
      std::hypot(points.back()[0] - points.front()[0], points.back()[1] - points.front()[1]);
  if (*first <= 0 || *first >= *last || *last >= length) {
    return std::unexpected(Error::NoSlopedRun);
  }
  planner.Output.Flights.push_back({.Source = ports.Source,
                                    .Start = ports.Start,
                                    .End = ports.End,
                                    .Direction = ports.Direction,
                                    .CutStart = *first,
                                    .CutEnd = *last,
                                    .Stations = std::move(stations)});
  planner.Output.Axes.push_back(
      {.Origin = axis->Origin, .Direction = axis->Direction, .Cuts = {*first, *last}});
  return {};
}
}

std::expected<SurfaceFlightPlan, SurfaceFlightError>
BuildSurfaceFlightPlan(const SurfaceHeightTopology &topology,
                       std::span<const SurfaceHeightRecipe> sources,
                       std::span<const SurfaceFlightEndpoints> flights,
                       SurfaceFlightBudget budget,
                       const std::stop_token &stop) {
  if (stop.stop_requested()) { return std::unexpected(Error::Canceled); }
  if (flights.size() > budget.Flights || topology.Points.size() > budget.Vertices ||
      sources.size() > std::numeric_limits<uint64_t>::digits) {
    return std::unexpected(Error::CapacityExceeded);
  }
  if (sources.size() != topology.Sources.size() || !std::isfinite(budget.StraightnessM) ||
      budget.StraightnessM < 0 || !std::isfinite(budget.LandingSearchM) ||
      budget.LandingSearchM <= 0) {
    return std::unexpected(Error::InvalidInput);
  }
  Planner planner{
      .Topology = topology, .Sources = sources, .Budget = budget, .Stop = stop, .Output = {}};
  std::vector<bool> owned(sources.size());
  for (const auto endpoints : flights) {
    if (endpoints.Source >= sources.size() || endpoints.Start >= topology.PortVertices.size() ||
        endpoints.End >= topology.PortVertices.size() || endpoints.Start == endpoints.End ||
        (endpoints.Direction != -1 && endpoints.Direction != 1) ||
        !sources[endpoints.Source].Steps) {
      return std::unexpected(Error::InvalidInput);
    }
    if (owned[endpoints.Source]) { return std::unexpected(Error::ConflictingOwners); }
    owned[endpoints.Source] = true;
    if (const auto result = AppendFlight(planner, endpoints); !result) {
      return std::unexpected(result.error());
    }
  }
  return std::move(planner.Output);
}
}
