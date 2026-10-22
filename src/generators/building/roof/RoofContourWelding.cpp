#include "RoofContourInternal.h"
#include <algorithm>
#include <expected>
#include <array>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <map>
#include <numbers>
#include <numeric>
#include <optional>
#include <set>
#include <span>
#include <utility>
#include <vector>

namespace outshine::Generators::RoofContour {
namespace {
constexpr double kMinimumWeldToleranceM = 1e-12;
constexpr double kMaximumWeldCellMagnitude = 0x1p62;

class VertexWeld {
  using Cell = std::array<int64_t, 2>;

public:
  VertexWeld(std::span<const Point> input, double tolerance)
      : Input_(input), Tolerance_(tolerance), Parent_(input.size()) {
    std::ranges::iota(Parent_, size_t{0});
  }

  Checked<std::vector<Point>> Run(size_t fixed) {
    for (size_t at = 0; at < Input_.size(); ++at) {
      const auto joined = Insert(at);
      if (!joined) { return std::unexpected(joined.error()); }
    }
    return Finish(fixed);
  }

private:
  size_t Root(size_t at) {
    while (Parent_[at] != at) {
      Parent_[at] = Parent_[Parent_[at]];
      at = Parent_[at];
    }
    return at;
  }

  [[nodiscard]] Checked<Cell> CellAt(Point point) const {
    Cell cell{};
    for (size_t axis = 0; axis < 2; ++axis) {
      const double coordinate = std::floor(point[axis] / Tolerance_);
      if (!std::isfinite(coordinate) || std::abs(coordinate) >= kMaximumWeldCellMagnitude) {
        return std::unexpected(Failure{HipRoofError::CapacityExceeded, "weld coordinate range"});
      }
      cell[axis] = static_cast<int64_t>(coordinate);
    }
    return cell;
  }

  Checked<void> Join(size_t at, size_t other) {
    if (++Candidates_ > kMaximumCandidates) {
      return std::unexpected(Failure{HipRoofError::CapacityExceeded, "weld candidate budget"});
    }
    const auto delta = Subtract(Input_[at], Input_[other]);
    if (std::hypot(delta[0], delta[1]) > Tolerance_) { return {}; }
    if (++Pairs_ > kMaximumWeldPairs) {
      return std::unexpected(Failure{HipRoofError::CapacityExceeded, "weld pair budget"});
    }
    const auto a = Root(at);
    const auto b = Root(other);
    Parent_[std::max(a, b)] = std::min(a, b);
    return {};
  }

  Checked<void> Insert(size_t at) {
    const auto cell = CellAt(Input_[at]);
    if (!cell) { return std::unexpected(cell.error()); }
    for (int dx = -1; dx <= 1; ++dx) {
      for (int dy = -1; dy <= 1; ++dy) {
        const auto found = Bins_.find({(*cell)[0] + dx, (*cell)[1] + dy});
        if (found == Bins_.end()) { continue; }
        for (const size_t other : found->second) {
          const auto joined = Join(at, other);
          if (!joined) { return std::unexpected(joined.error()); }
        }
      }
    }
    Bins_[*cell].push_back(at);
    return {};
  }

  Checked<std::vector<Point>> Finish(size_t fixed) {
    std::set<size_t> fixedRoots;
    std::vector<Point> result;
    result.reserve(Input_.size());
    for (size_t at = 0; at < Input_.size(); ++at) {
      const auto first = Root(at);
      if (at < fixed && !fixedRoots.insert(first).second) {
        return std::unexpected(Failure{"fixed vertices merged"});
      }
      const auto delta = Subtract(Input_[at], Input_[first]);
      if (std::hypot(delta[0], delta[1]) > Tolerance_) {
        return std::unexpected(Failure{"transitive weld displacement"});
      }
      result.push_back(Input_[first]);
    }
    return result;
  }

  std::span<const Point> Input_;
  double Tolerance_;
  std::vector<size_t> Parent_;
  std::map<Cell, std::vector<size_t>> Bins_;
  size_t Pairs_ = 0;
  size_t Candidates_ = 0;
};
}

Checked<std::vector<Point>>
WeldVertices(std::span<const Point> input, double tolerance, size_t fixed) {
  if (input.empty() || input.size() > kMaximumWeldVertices || !std::isfinite(tolerance) ||
      tolerance < kMinimumWeldToleranceM || tolerance > 1 || fixed > input.size()) {
    return std::unexpected(Failure{HipRoofError::CapacityExceeded, "weld input/budget"});
  }
  return VertexWeld(input, tolerance).Run(fixed);
}

std::vector<Point> Unique(std::span<const Point> input) {
  std::set<Point> unique(input.begin(), input.end());
  return {unique.begin(), unique.end()};
}

Checked<std::vector<Point>>
Subdivide(std::span<const Point> input, std::span<const Point> anchors, double tolerance) {
  if (!input.empty() && anchors.size() > kMaximumCandidates / input.size()) {
    return std::unexpected(Failure{HipRoofError::CapacityExceeded, "subdivision candidate budget"});
  }
  std::vector<Point> result;
  for (size_t at = 0; at < input.size(); ++at) {
    const auto start = input[at];
    const auto delta = Subtract(input[(at + 1) % input.size()], start);
    const double length = std::hypot(delta[0], delta[1]);
    if (length <= tolerance) { continue; }
    std::vector<std::pair<double, Point>> entries{{0, start}};
    for (const auto anchor : anchors) {
      const auto d = Subtract(anchor, start);
      const double parameter = Dot(d, delta) / (length * length);
      const double distance = std::abs(d[0] * delta[1] - d[1] * delta[0]) / length;
      const double guard = tolerance / length;
      if (parameter > guard && parameter < 1 - guard && distance <= tolerance) {
        entries.emplace_back(parameter, anchor);
      }
    }
    std::ranges::stable_sort(entries,
                             [](const auto &a, const auto &b) { return a.first < b.first; });
    for (const auto &entry : entries) { result.push_back(entry.second); }
  }
  return result;
}
}
