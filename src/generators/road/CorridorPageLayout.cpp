#include "Corridors.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <limits>
#include <map>
#include <numeric>
#include <span>
#include <string_view>
#include <utility>
#include <vector>

namespace outshine::Generators {
namespace {
using CellKey = std::pair<int64_t, int64_t>;

std::expected<CellKey, std::string_view> Cell(const auto &points, double width) {
  if (points.empty()) { return std::unexpected("road page part has no planned position"); }
  std::array<double, 2> low{points.front().EastM, points.front().NorthM};
  auto high = low;
  for (const auto &point : points) {
    const std::array<double, 2> value{point.EastM, point.NorthM};
    for (size_t axis = 0; axis != 2; ++axis) {
      if (!std::isfinite(value[axis])) {
        return std::unexpected("road page part has a nonfinite planned position");
      }
      low[axis] = std::min(low[axis], value[axis]);
      high[axis] = std::max(high[axis], value[axis]);
    }
  }
  std::array<int64_t, 2> key{};
  constexpr auto lowest = static_cast<double>(std::numeric_limits<int64_t>::min());
  for (size_t axis = 0; axis != 2; ++axis) {
    const double value = std::floor(std::midpoint(low[axis], high[axis]) / width);
    if (!std::isfinite(value) || value < lowest || value >= -lowest) {
      return std::unexpected("road page cell exceeds its signed coordinate range");
    }
    key[axis] = static_cast<int64_t>(value);
  }
  return CellKey{key[0], key[1]};
}

bool Before(const RoadPagePart &left, const RoadPagePart &right) {
  return left.Kind != right.Kind ? left.Kind < right.Kind : left.Index < right.Index;
}
}

std::expected<std::vector<std::vector<RoadPagePart>>, std::string_view>
Corridors::PlannedPages(const Job &job, double cellWidthM, size_t partsMost) {
  if (!std::isfinite(cellWidthM) || cellWidthM <= 0 || partsMost == 0) {
    return std::unexpected("road page layout needs positive cell width and part limit");
  }
  auto parts = PlannedParts(job);
  if (!parts) { return std::unexpected(parts.error()); }
  const auto &work = job.Work;
  std::map<CellKey, std::vector<RoadPagePart>> cells;
  for (const auto part : *parts) {
    std::expected<CellKey, std::string_view> cell = std::unexpected("unknown road page part");
    if (part.Kind == RoadPagePartKind::Interval) {
      const auto &edge = work.Edges[part.Index];
      if (edge.PlannedFirst > work.Planned.size() ||
          edge.PlannedCount > work.Planned.size() - edge.PlannedFirst) {
        return std::unexpected("road page interval exceeds its complete plan storage");
      }
      cell =
          Cell(std::span(work.Planned).subspan(edge.PlannedFirst, edge.PlannedCount), cellWidthM);
    } else {
      const auto &junction = work.Junctions[part.Index];
      const std::array<RoadStation, 1> point{
          {{.EastM = junction.EastM, .NorthM = junction.NorthM}}};
      cell = junction.Gates.empty() ? Cell(point, cellWidthM) : Cell(junction.Gates, cellWidthM);
    }
    if (!cell) { return std::unexpected(cell.error()); }
    cells[*cell].push_back(part);
  }
  std::vector<std::span<const RoadPagePart>> ordered;
  for (const auto &[cell, members] : cells) {
    for (size_t first = 0; first != members.size();) {
      const size_t count = std::min(partsMost, members.size() - first);
      const auto selected = std::span(members).subspan(first, count);
      ordered.push_back(selected);
      first += count;
    }
  }
  std::ranges::sort(ordered, [](const auto &left, const auto &right) {
    return Before(left.front(), right.front());
  });
  std::vector<std::vector<RoadPagePart>> result;
  result.reserve(ordered.size());
  for (const auto page : ordered) { result.emplace_back(page.begin(), page.end()); }
  return result;
}
}
