#include "SurfaceBodyInternal.h"
#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <map>
#include <span>
#include <tuple>
#include <utility>
#include <vector>

namespace outshine::Generators::SurfaceBodies {
namespace {
Checked<uint32_t>
Plane(State &state, uint32_t faceIndex, const Flight &flight, const SurfaceTreadRecipe &recipe) {
  const auto face = state.Output.ReferenceFaces[faceIndex];
  std::array<double, 3> stations{};
  const double first = flight.Direction > 0 ? recipe.LowM : recipe.HighM;
  const double last = flight.Direction > 0 ? recipe.HighM : recipe.LowM;
  for (size_t corner = 0; corner < face.size(); ++corner) {
    if (auto valid = state.Visit(); !valid) { return std::unexpected(valid.error()); }
    const auto point = state.Output.ReferencePoints[face[corner]];
    const double station = (point[0] - flight.Origin[0]) * flight.Tangent[0] +
                           (point[1] - flight.Origin[1]) * flight.Tangent[1];
    if (!std::isfinite(station)) { return std::unexpected(Error::InvalidInput); }
    stations[corner] = station;
    const double ratio =
        std::clamp((station - flight.Start) / (flight.End - flight.Start), 0.0, 1.0);
    const double residual = std::abs(point[2] - (first + ratio * (last - first)));
    ++state.Output.Work.PlaneTests;
    state.Output.Work.MaximumPlaneErrorM = std::max(state.Output.Work.MaximumPlaneErrorM, residual);
    if (!std::isfinite(residual) || residual > SurfaceBodyBudget::kMaximumPlaneErrorM) {
      return std::unexpected(Error::NonplanarFlight);
    }
  }
  const double mean = (stations[0] + stations[1] + stations[2]) / 3;
  const auto [low, high] = std::ranges::minmax(stations);
  uint32_t region = 1;
  if (mean <= flight.Start) {
    region = 0;
  } else if (mean >= flight.End) {
    region = 2;
  }
  constexpr double tolerance = SurfaceBodyBudget::kMaximumPlaneErrorM;
  if ((region == 0 && high > flight.Start + tolerance) ||
      (region == 2 && low < flight.End - tolerance) ||
      (region == 1 && (low < flight.Start - tolerance || high > flight.End + tolerance))) {
    return std::unexpected(Error::CrossingPlane);
  }
  return region;
}

Checked<void>
BandInterval(const State &state, uint32_t faceIndex, const SurfaceTreadRecipe &recipe) {
  const auto level = state.Output.FaceLevels[faceIndex];
  if (std::cmp_less(level, recipe.FirstLevel) ||
      std::cmp_greater(level, recipe.FirstLevel + recipe.Risers)) {
    return std::unexpected(Error::ConflictingOwners);
  }
  const double high = state.Output.Levels[static_cast<size_t>(level)];
  const double low = std::cmp_greater(level, recipe.FirstLevel)
                         ? state.Output.Levels[static_cast<size_t>(level) - 1]
                         : high;
  for (const auto vertex : state.Output.ReferenceFaces[faceIndex]) {
    const double height = state.Output.ReferencePoints[vertex][2];
    if (height < low - SurfaceBodyBudget::kMaximumPlaneErrorM ||
        height > high + SurfaceBodyBudget::kMaximumPlaneErrorM) {
      return std::unexpected(Error::ConflictingOwners);
    }
  }
  return {};
}

Checked<HeightLevelRange> FaceLevels(State &state, uint32_t faceIndex) {
  const uint64_t owners = state.Output.FaceSources[faceIndex];
  uint32_t chosen = kNone;
  for (uint32_t at = 0; at < state.Output.Stairs.size(); ++at) {
    if ((owners & (uint64_t{1} << state.Output.Stairs[at].Source)) == 0) { continue; }
    if (chosen != kNone) { return std::unexpected(Error::ConflictingOwners); }
    chosen = at;
  }
  if (chosen == kNone) { return HeightLevelRange{}; }
  const auto &recipe = state.Output.Stairs[chosen];
  const auto face = state.Output.ReferenceFaces[faceIndex];
  const std::array z{state.Output.ReferencePoints[face[0]][2],
                     state.Output.ReferencePoints[face[1]][2],
                     state.Output.ReferencePoints[face[2]][2]};
  const auto [low, high] = std::ranges::minmax(z);
  if (low < recipe.LowM - SurfaceBodyBudget::kLandingIntervalM ||
      high > recipe.HighM + SurfaceBodyBudget::kLandingIntervalM) {
    return std::unexpected(Error::ConflictingOwners);
  }
  if (high - low < SurfaceBodyBudget::kFlatIntervalM) {
    const double mean = (z[0] + z[1] + z[2]) / 3;
    const double offset =
        std::ceil((mean - recipe.LowM) / recipe.RiseM - SurfaceBodyBudget::kLevelSelectionBias);
    const auto level =
        static_cast<uint32_t>(std::clamp(offset, 0.0, static_cast<double>(recipe.Risers)));
    return HeightLevelRange{.First = recipe.FirstLevel + level, .Count = 1};
  }
  if (!std::has_single_bit(owners)) { return std::unexpected(Error::ConflictingOwners); }
  return HeightLevelRange{.First = recipe.FirstLevel, .Count = recipe.Risers + 1};
}
}

Checked<std::vector<HeightLevelRange>> AssignLevels(State &state) {
  std::vector<HeightLevelRange> levels;
  levels.reserve(state.Output.ReferenceFaces.size());
  for (uint32_t at = 0; at < state.Output.ReferenceFaces.size(); ++at) {
    if (auto valid = state.Visit(); !valid) { return std::unexpected(valid.error()); }
    const auto range = FaceLevels(state, at);
    if (!range) { return std::unexpected(range.error()); }
    levels.push_back(*range);
  }
  return levels;
}

Checked<Groups> GroupFaces(State &state, Pass pass) {
  using Key = std::tuple<uint64_t, int32_t, uint32_t>;
  std::map<Key, uint32_t> lookup;
  Groups output;
  for (uint32_t at = 0; at < state.Output.ReferenceFaces.size(); ++at) {
    if (auto valid = state.Visit(); !valid) { return std::unexpected(valid.error()); }
    const auto owners = state.Output.FaceSources[at];
    const auto source = static_cast<uint32_t>(std::countr_zero(owners));
    const auto flight = source < state.SourceFlights.size() ? state.SourceFlights[source] : kNone;
    const int32_t level = pass == Pass::Bands ? state.Output.FaceLevels[at] : -1;
    if (!std::has_single_bit(owners) || flight == kNone || (pass == Pass::Bands && level < 0)) {
      output.Passed.push_back(at);
      continue;
    }
    if (pass == Pass::Bands) {
      if (auto valid = BandInterval(state, at, state.Output.Stairs[flight]); !valid) {
        return std::unexpected(valid.error());
      }
    }
    const auto region = Plane(state, at, state.Flights[flight], state.Output.Stairs[flight]);
    if (!region) { return std::unexpected(region.error()); }
    const Key key{owners, level, *region};
    const auto [entry, added] =
        lookup.try_emplace(key, static_cast<uint32_t>(output.Regions.size()));
    if (added) {
      output.Regions.emplace_back();
      output.Leaders.push_back(at);
    }
    output.Regions[entry->second].push_back(at);
  }
  return output;
}
}
