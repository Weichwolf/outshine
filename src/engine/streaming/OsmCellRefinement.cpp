#include "OsmSourceLoaderState.h"

#include <algorithm>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <span>
#include <string>
#include <utility>
#include <vector>
#include <optional>
#include <ranges>
#include <tuple>

namespace outshine {
namespace {
bool Before(Data::GeoCellId left, Data::GeoCellId right) noexcept {
  return std::tie(left.Level, left.X, left.Y) < std::tie(right.Level, right.X, right.Y);
}

std::optional<size_t> RootPosition(std::span<const Data::GeoCellId> roots, Data::GeoCellId leaf) {
  for (;;) {
    const auto at = std::ranges::lower_bound(roots, leaf, Before);
    if (at != roots.end() && *at == leaf) { return static_cast<size_t>(at - roots.begin()); }
    if (leaf.Level == 0) { return std::nullopt; }
    --leaf.Level;
    leaf.X /= 2;
    leaf.Y /= 2;
  }
}
}

std::vector<Data::GeoCellId> OsmSourceLoader::Cells::SelectLeaves(
    std::span<const Data::GeoCellId> roots, bool published, bool pending) const {
  std::vector<std::vector<Data::GeoCellId>> plans(roots.size());
  if (pending) {
    for (const auto leaf : Wanted) {
      const auto at = RootPosition(roots, leaf);
      if (at && std::ranges::binary_search(Roots, roots[*at], Before)) {
        plans[*at].push_back(leaf);
      }
    }
  }
  if (published) {
    std::vector<bool> use(roots.size());
    for (size_t at = 0; at < roots.size(); ++at) {
      use[at] = plans[at].empty() && std::ranges::binary_search(PublishedRoots, roots[at], Before);
    }
    for (const auto &entry : Published) {
      const auto cell = entry.Snapshot->Cell;
      assert(cell);
      const auto at = RootPosition(roots, *cell);
      if (at && use[*at]) { plans[*at].push_back(leaf); }
    }
  }
  std::vector<Data::GeoCellId> leaves;
  for (size_t at = 0; at < roots.size(); ++at) {
    if (plans[at].empty()) {
      leaves.push_back(roots[at]);
    } else {
      leaves.insert(leaves.end(), plans[at].begin(), plans[at].end());
    }
  }
  std::ranges::sort(leaves, Before);
  return leaves;
}

std::expected<void, std::string>
OsmSourceLoader::Cells::Refine(std::span<const Data::GeoCellId> cells) {
  if (cells.empty()) { return {}; }
  if (cells.size() > (Limits.CellsMost - Wanted.size()) / 3) {
    return std::unexpected("original OSM subdivision exceeds the cell budget");
  }
  for (size_t index = 0; index < cells.size(); ++index) {
    const auto cell = cells[index];
    const auto at = std::ranges::find(Wanted, cell);
    if (at == Wanted.end() || Preparing[static_cast<size_t>(at - Wanted.begin())].Snapshot ||
        std::ranges::find(cells.first(index), cell) != cells.first(index).end()) {
      return std::unexpected("original OSM subdivision does not belong to pending demand");
    }
    if (cell.Level >= Limits.LevelMost) {
      return std::unexpected("original OSM subdivision exceeds the level budget");
    }
  }
  auto leaves = Wanted;
  leaves.reserve(Wanted.size() + cells.size() * 3);
  for (const auto cell : cells) {
    std::erase(leaves, cell);
    for (uint32_t x = 0; x < 2; ++x) {
      for (uint32_t y = 0; y < 2; ++y) {
        leaves.push_back({.Level = cell.Level + 1, .X = cell.X * 2 + x, .Y = cell.Y * 2 + y});
      }
    }
  }
  std::ranges::sort(leaves, Before);
  auto preparing = Reuse(leaves, false, true);
  Wanted = std::move(leaves);
  Preparing = std::move(preparing);
  return {};
}

}
