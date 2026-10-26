#include "PrismGridViews.h"
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <limits>
#include <span>
#include <vector>

namespace outshine {
namespace {
struct Interval {
  uint32_t Begin = 0, End = 0;
};

bool Intersects(const PrismCellRect &cut, const PrismPatch &patch) {
  return cut.Left < cut.Right && cut.Top < cut.Bottom && cut.Left < patch.Columns &&
         cut.Top < patch.Rows;
}

bool Fits(const PrismPatch &patch, size_t samples) {
  return patch.Columns != 0 && patch.Rows != 0 && patch.FirstSample <= samples &&
         uint64_t{patch.Columns} * patch.Rows <= samples - patch.FirstSample;
}

void Append(PrismGridViews &out, const PrismPatch &source, PrismCellRect region) {
  auto shape = source;
  shape.FirstSample += uint64_t{region.Top} * source.Columns + region.Left;
  shape.Columns = region.Right - region.Left;
  shape.Rows = region.Bottom - region.Top;
  out.Geometry.Patches.push_back(shape);
  out.Placements.push_back({.Patch = region.Patch, .Column = region.Left, .Row = region.Top});
}

std::expected<void, PrismViewError> Emit(PrismGridViews &out,
                                         const PrismPatch &source,
                                         uint32_t index,
                                         Interval rows,
                                         std::span<const Interval> visible,
                                         size_t viewsMost) {
  if (visible.empty()) { return {}; }
  const bool fullWidth =
      visible.size() == 1 && visible.front().Begin == 0 && visible.front().End == source.Columns;
  const uint64_t copies = fullWidth ? 1 : rows.End - rows.Begin;
  if (copies != 0 && visible.size() > (viewsMost - out.Geometry.Patches.size()) / copies) {
    return std::unexpected(PrismViewError::OverBudget);
  }
  if (fullWidth) {
    Append(out,
           source,
           {.Patch = index,
            .Left = 0,
            .Top = rows.Begin,
            .Right = source.Columns,
            .Bottom = rows.End});
  } else {
    for (uint64_t row = rows.Begin; row < rows.End; ++row) {
      const auto begin = static_cast<uint32_t>(row);
      for (const auto columns : visible) {
        Append(out,
               source,
               {.Patch = index,
                .Left = columns.Begin,
                .Top = begin,
                .Right = columns.End,
                .Bottom = begin + 1});
      }
    }
  }
  return {};
}

std::expected<std::vector<Interval>, PrismViewError>
VisibleColumns(std::span<const PrismCellRect> cuts,
               const PrismPatch &source,
               uint32_t row,
               PrismGridViews &out,
               size_t testsMost) {
  if (cuts.size() > testsMost - out.IntersectionTests) {
    return std::unexpected(PrismViewError::OverBudget);
  }
  out.IntersectionTests += cuts.size();
  std::vector<Interval> blocked;
  blocked.reserve(cuts.size());
  for (const auto &cut : cuts) {
    if (row >= cut.Top && row < cut.Bottom && cut.Left < source.Columns && cut.Left < cut.Right) {
      blocked.push_back({.Begin = cut.Left, .End = std::min(cut.Right, source.Columns)});
    }
  }
  std::ranges::sort(blocked, {}, &Interval::Begin);
  std::vector<Interval> visible;
  uint32_t first = 0;
  for (const auto interval : blocked) {
    if (interval.Begin > first) { visible.push_back({.Begin = first, .End = interval.Begin}); }
    first = std::max(first, interval.End);
  }
  if (first < source.Columns) { visible.push_back({.Begin = first, .End = source.Columns}); }
  return visible;
}

std::expected<void, PrismViewError> ClipPatch(PrismGridViews &out,
                                              const PrismPatch &source,
                                              uint32_t index,
                                              std::span<const PrismCellRect> cuts,
                                              PrismViewLimits limits) {
  if (cuts.empty()) {
    if (out.Geometry.Patches.size() == limits.ViewsMost) {
      return std::unexpected(PrismViewError::OverBudget);
    }
    Append(out,
           source,
           {.Patch = index, .Left = 0, .Top = 0, .Right = source.Columns, .Bottom = source.Rows});
    return {};
  }
  std::vector<uint32_t> edges{0, source.Rows};
  for (const auto &cut : cuts) {
    edges.push_back(std::min(cut.Top, source.Rows));
    edges.push_back(std::min(cut.Bottom, source.Rows));
  }
  std::ranges::sort(edges);
  const auto redundant = std::ranges::unique(edges);
  edges.erase(redundant.begin(), redundant.end());
  for (size_t band = 1; band < edges.size(); ++band) {
    const auto visible =
        VisibleColumns(cuts, source, edges[band - 1], out, limits.IntersectionTestsMost);
    if (!visible) { return std::unexpected(visible.error()); }
    const auto emitted = Emit(out,
                              source,
                              index,
                              {.Begin = edges[band - 1], .End = edges[band]},
                              *visible,
                              limits.ViewsMost);
    if (!emitted) { return emitted; }
  }
  return {};
}
}

std::expected<PrismGridViews, PrismViewError> ExcludePrismCells(
    const PrismGrid &source, std::span<const PrismCellRect> exclusions, PrismViewLimits limits) {
  if (source.Patches.size() > limits.InputRecordsMost ||
      exclusions.size() > limits.InputRecordsMost - source.Patches.size() ||
      source.Patches.size() > std::numeric_limits<uint32_t>::max() ||
      exclusions.size() > limits.IntersectionTestsMost / 2) {
    return std::unexpected(PrismViewError::OverBudget);
  }
  const size_t samples = source.Samples ? source.Samples->size() : 0;
  for (const auto &patch : source.Patches) {
    if (!Fits(patch, samples)) { return std::unexpected(PrismViewError::InvalidSource); }
  }
  std::vector<size_t> offsets(source.Patches.size() + 1);
  for (const auto &cut : exclusions) {
    if (cut.Patch >= source.Patches.size() || cut.Left > cut.Right || cut.Top > cut.Bottom) {
      return std::unexpected(PrismViewError::InvalidExclusion);
    }
    if (Intersects(cut, source.Patches[cut.Patch])) { ++offsets[cut.Patch + 1]; }
  }
  for (size_t index = 1; index < offsets.size(); ++index) { offsets[index] += offsets[index - 1]; }
  auto next = offsets;
  std::vector<PrismCellRect> ordered(offsets.back());
  for (const auto &cut : exclusions) {
    if (Intersects(cut, source.Patches[cut.Patch])) { ordered[next[cut.Patch]++] = cut; }
  }
  PrismGridViews result{.Geometry = {.Samples = source.Samples, .Patches = {}},
                        .Placements = {},
                        .IntersectionTests = exclusions.size() * 2};
  for (size_t index = 0; index < source.Patches.size(); ++index) {
    const auto clipped =
        ClipPatch(result,
                  source.Patches[index],
                  static_cast<uint32_t>(index),
                  std::span(ordered).subspan(offsets[index], offsets[index + 1] - offsets[index]),
                  limits);
    if (!clipped) { return std::unexpected(clipped.error()); }
  }
  return result;
}
}
