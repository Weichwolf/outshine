#include "MvtGroundFieldScopeInternal.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <span>
#include <vector>

namespace outshine::Generators::Osm::GroundFieldScopes {
namespace {
bool Interior(State &state, const World::TransportNode &node) {
  uint8_t quadrants = 0;
  for (const auto &box : state.Source().Source->Coverage()) {
    const bool south = box.SouthDeg < node.LatitudeDeg && node.LatitudeDeg <= box.NorthDeg;
    const bool north = box.SouthDeg <= node.LatitudeDeg && node.LatitudeDeg < box.NorthDeg;
    for (const double offset : {-360., 0., 360.}) {
      ++state.Output.Work.CoverageTests;
      const bool left =
          box.WestDeg + offset < node.LongitudeDeg && node.LongitudeDeg <= box.EastDeg + offset;
      const bool right =
          box.WestDeg + offset <= node.LongitudeDeg && node.LongitudeDeg < box.EastDeg + offset;
      quadrants = static_cast<uint8_t>(quadrants | static_cast<uint8_t>(left && south) |
                                       (static_cast<uint8_t>(left && north) << 1U) |
                                       (static_cast<uint8_t>(right && south) << 2U) |
                                       (static_cast<uint8_t>(right && north) << 3U));
      if (quadrants == 15) { return true; }
    }
  }
  return false;
}

bool Interval(const std::array<double, 2> &a,
              const std::array<double, 2> &delta,
              const GeographicBounds &box,
              double offset,
              std::array<double, 2> &interval) {
  const std::array low{box.WestDeg + offset, box.SouthDeg};
  const std::array high{box.EastDeg + offset, box.NorthDeg};
  interval = {0, 1};
  for (size_t axis = 0; axis < 2; ++axis) {
    if (delta[axis] == 0) {
      if (a[axis] < low[axis] || a[axis] > high[axis]) { return false; }
    } else {
      const auto x = (low[axis] - a[axis]) / delta[axis];
      const auto y = (high[axis] - a[axis]) / delta[axis];
      interval[0] = std::max(interval[0], std::min(x, y));
      interval[1] = std::min(interval[1], std::max(x, y));
    }
  }
  return interval[0] <= interval[1];
}

bool Segment(State &state,
             const World::TransportNode &from,
             const World::TransportNode &to,
             std::vector<std::array<double, 2>> &intervals) {
  intervals.clear();
  const std::array a{from.LongitudeDeg, from.LatitudeDeg};
  const std::array delta{std::fmod(to.LongitudeDeg - from.LongitudeDeg + 540., 360.) - 180.,
                         to.LatitudeDeg - from.LatitudeDeg};
  for (const auto &box : state.Source().Source->Coverage()) {
    for (const double offset : {-360., 0., 360.}) {
      ++state.Output.Work.CoverageTests;
      std::array<double, 2> interval{};
      if (Interval(a, delta, box, offset, interval)) { intervals.push_back(interval); }
    }
  }
  std::ranges::sort(intervals);
  double covered = 0;
  for (const auto &interval : intervals) {
    if (interval[0] > covered) { return false; }
    covered = std::max(covered, interval[1]);
  }
  return covered >= 1;
}
}

Checked<void> Coverage(State &state) {
  const auto &graph = state.Source().Source->Topology();
  if (state.Source().Source->Coverage().empty()) { return std::unexpected(Error::CoverageGap); }
  for (const auto node : state.Source().Nodes) {
    if (state.Stop.stop_requested()) { return std::unexpected(Error::Canceled); }
    if (!Interior(state, graph.Nodes()[node])) { return std::unexpected(Error::CoverageGap); }
  }
  std::vector<std::array<double, 2>> intervals;
  intervals.reserve(state.Source().Source->Coverage().size() * 3);
  for (size_t at = 1; at < state.Source().Nodes.size(); ++at) {
    if (state.Stop.stop_requested()) { return std::unexpected(Error::Canceled); }
    if (!Segment(state,
                 graph.Nodes()[state.Source().Nodes[at - 1]],
                 graph.Nodes()[state.Source().Nodes[at]],
                 intervals)) {
      return std::unexpected(Error::CoverageGap);
    }
  }
  state.Output.Work.ContainerCapacityBytes += intervals.capacity() * sizeof(std::array<double, 2>);
  return {};
}
}
