#include "PreparedBuildingAssets.h"
#include "PageIndexAsset.h"
#include "PreparedStructureTile.h"
#include <bit>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <string>
#include <vector>
#include <optional>
#include <utility>

namespace outshine::Generators {
std::expected<std::vector<PreparedBuildingAssets::SourceCell>, StructureBakeError>
PreparedBuildingAssets::DescribeSourceCells(const std::string &key, uint64_t occupied) const {
  std::vector<SourceCell> cells;
  const auto parent = Cache_->Find(key);
  if (!parent) { return std::unexpected(CacheFailure(parent.error())); }
  if (!*parent) { return cells; }
  cells.reserve(static_cast<size_t>(std::popcount(occupied)));
  while (occupied != 0) {
    const uint32_t cell = static_cast<uint32_t>(std::countr_zero(occupied)) + 1u;
    occupied &= occupied - 1u;
    const auto found = Cache_->Find(SurfaceKey(key, cell));
    if (!found) { return std::unexpected(CacheFailure(found.error())); }
    if (!*found) { continue; }
    const auto &record = **found;
    if (record.Kind != "building-forms" || record.Parent != key || record.Level != 1) {
      return std::unexpected(StructureBakeErrorKind::ArtifactInvalidProduct);
    }
    cells.push_back({.Cell = cell,
                     .Bounds = record.Bounds,
                     .NativeBytes = record.ByteCount,
                     .HeaderBytes = (**parent).ByteCount});
  }
  return cells;
}
}
