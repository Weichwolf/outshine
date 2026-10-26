#include "SurfaceBodyInternal.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <limits>
#include <map>
#include <span>
#include <utility>
#include <vector>

namespace outshine::Generators::SurfaceBodies {
namespace {
Checked<void> Caps(State &state) {
  const auto &topology = state.Request.Topology;
  std::map<PlanarFace, uint32_t> lookup;
  for (uint32_t source = 0; source < topology.Sources.size(); ++source) {
    for (const auto face : topology.Sources[source].Faces) {
      if (auto valid = state.Visit(face.size()); !valid) { return valid; }
      for (const auto vertex : face) {
        if (vertex >= topology.Points.size()) { return std::unexpected(Error::InvalidInput); }
      }
      const auto [entry, added] =
          lookup.try_emplace(face, static_cast<uint32_t>(state.Output.ReferenceFaces.size()));
      if (added) {
        if (state.Output.ReferenceFaces.size() >= state.Budget.Faces) {
          return std::unexpected(Error::CapacityExceeded);
        }
        state.Output.ReferenceFaces.push_back(face);
        state.Output.FaceSources.push_back(0);
      }
      state.Output.FaceSources[entry->second] |= uint64_t{1} << source;
    }
  }
  return {};
}

Checked<std::array<uint32_t, 2>>
Nearest(State &state, uint32_t source, std::array<uint32_t, 2> ports) {
  const std::array origins{state.Request.Ports[ports[0]].Origin,
                           state.Request.Ports[ports[1]].Origin};
  for (const auto origin : origins) {
    if (!std::isfinite(origin[0]) || !std::isfinite(origin[1])) {
      return std::unexpected(Error::InvalidInput);
    }
  }
  std::vector<bool> present(state.Output.ReferencePoints.size());
  for (size_t at = 0; at < state.Output.ReferenceFaces.size(); ++at) {
    if (auto valid = state.Visit(); !valid) { return std::unexpected(valid.error()); }
    if ((state.Output.FaceSources[at] & (uint64_t{1} << source)) == 0) { continue; }
    for (const auto vertex : state.Output.ReferenceFaces[at]) { present[vertex] = true; }
  }
  std::array best{kNone, kNone};
  std::array distances{std::numeric_limits<double>::infinity(),
                       std::numeric_limits<double>::infinity()};
  for (uint32_t at = 0; at < present.size(); ++at) {
    if (auto valid = state.Visit(); !valid) { return std::unexpected(valid.error()); }
    if (!present[at]) { continue; }
    const auto point = state.Output.ReferencePoints[at];
    for (size_t end = 0; end < origins.size(); ++end) {
      ++state.Output.Work.NearestTests;
      const double dx = point[0] - origins[end][0];
      const double dy = point[1] - origins[end][1];
      const double candidate = std::sqrt(dx * dx + dy * dy);
      if (candidate < distances[end]) {
        distances[end] = candidate;
        best[end] = at;
      }
    }
  }
  if (best[0] == kNone || best[1] == kNone) { return std::unexpected(Error::MissingLanding); }
  return best;
}

Checked<Flight> Axis(State &state, const SurfaceHeightFlight &flight) {
  const auto &axis = state.Request.Sources[flight.Source].Axis;
  if (axis.size() < 2) { return std::unexpected(Error::InvalidInput); }
  const auto origin = axis.front();
  PlanarPoint tangent{axis.back()[0] - origin[0], axis.back()[1] - origin[1]};
  const double length = std::sqrt(tangent[0] * tangent[0] + tangent[1] * tangent[1]);
  if (!std::isfinite(length) || length <= 0) { return std::unexpected(Error::InvalidInput); }
  for (auto &value : tangent) { value /= length; }
  for (const auto point : axis) {
    if (auto valid = state.Visit(); !valid) { return std::unexpected(valid.error()); }
    const double side = (point[0] - origin[0]) * -tangent[1] + (point[1] - origin[1]) * tangent[0];
    if (!std::isfinite(side) || std::abs(side) > SurfaceBodyBudget::kMaximumPlaneErrorM) {
      return std::unexpected(Error::NonplanarFlight);
    }
  }
  return Flight{.Origin = origin,
                .Tangent = tangent,
                .Start = flight.CutStart,
                .End = flight.CutEnd,
                .Direction = flight.Direction};
}

Checked<void> AddFlight(State &state, const SurfaceHeightFlight &flight) {
  if (!state.Request.Sources[flight.Source].Steps || state.SourceFlights[flight.Source] != kNone) {
    return std::unexpected(Error::ConflictingOwners);
  }
  const auto ends = Nearest(state, flight.Source, {flight.Start, flight.End});
  if (!ends) { return std::unexpected(ends.error()); }
  const double low = state.Output.ReferencePoints[(*ends)[0]][2];
  const double high = state.Output.ReferencePoints[(*ends)[1]][2];
  const double count = std::ceil((high - low) / state.Budget.MaximumRiseM);
  if (!std::isfinite(count) || count < 1 || count > static_cast<double>(state.Budget.Risers)) {
    return std::unexpected(Error::UnsupportedRise);
  }
  const auto axis = Axis(state, flight);
  if (!axis) { return std::unexpected(axis.error()); }
  const auto risers = static_cast<uint32_t>(count);
  if (risers + 1 > state.Budget.Bands.Levels - state.Output.Levels.size()) {
    return std::unexpected(Error::CapacityExceeded);
  }
  state.SourceFlights[flight.Source] = static_cast<uint32_t>(state.Output.Stairs.size());
  state.Output.Stairs.push_back({.Source = flight.Source,
                                 .FirstLevel = static_cast<uint32_t>(state.Output.Levels.size()),
                                 .Risers = risers,
                                 .LowM = low,
                                 .HighM = high,
                                 .RiseM = (high - low) / count});
  const double rise = (high - low) / count;
  for (uint32_t at = 0; at <= risers; ++at) {
    state.Output.Levels.push_back(at == risers ? high : low + static_cast<double>(at) * rise);
  }
  state.Flights.push_back(*axis);
  return {};
}
}

Checked<void> RefreshRecipes(State &state) {
  const auto original = std::move(state.Output.Stairs);
  state.Output.Stairs.clear();
  state.Output.Levels.clear();
  state.Flights.clear();
  state.SourceFlights.assign(state.Request.Sources.size(), kNone);
  for (const auto &flight : state.Request.Flights) {
    if (auto added = AddFlight(state, flight); !added) { return added; }
  }
  if (state.Output.Stairs != original) { return std::unexpected(Error::ConflictingOwners); }
  return {};
}

Checked<void> Prepare(State &state) {
  if (auto valid = Validate(state); !valid) { return valid; }
  state.Output.ReferencePoints.reserve(state.Heights.size());
  for (size_t at = 0; at < state.Heights.size(); ++at) {
    if (auto valid = state.Visit(); !valid) { return valid; }
    const auto xy = state.Request.Topology.Points[at];
    state.Output.ReferencePoints.push_back({xy[0], xy[1], state.Heights[at]});
  }
  if (auto valid = Caps(state); !valid) { return valid; }
  state.SourceFlights.assign(state.Request.Sources.size(), kNone);
  for (const auto &flight : state.Request.Flights) {
    if (auto added = AddFlight(state, flight); !added) { return added; }
  }
  for (size_t source = 0; source < state.SourceFlights.size(); ++source) {
    if (state.Request.Sources[source].Steps && state.SourceFlights[source] == kNone) {
      return std::unexpected(Error::ConflictingOwners);
    }
  }
  return {};
}
}
