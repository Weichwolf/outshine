#include "BuildingSourceMetadata.h"
#include "PreparedBuildingBasis.h"
#include "BinaryValueArchive.h"
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <span>
#include <utility>
#include <vector>

namespace outshine::Generators {
namespace {
constexpr uint64_t kFormat = 0x31524353444C4942ULL;
}

std::optional<BuildingSourceMetadata>
BuildingSourceMetadata::Of(const PreparedBuildingBasis &basis) {
  BuildingSourceMetadata result{.AnchorEcef = basis.AnchorEcef,
                                .HeightQualified = basis.HeightQualified};
  for (const auto &source : basis.Sources) {
    if (source.Cell == 0 || source.Cell > kStructureCellsPerTile) { return std::nullopt; }
    auto &count = result.CellSourceCounts[source.Cell - 1u];
    if (count == std::numeric_limits<uint32_t>::max()) { return std::nullopt; }
    ++count;
    result.Cells |= uint64_t{1} << (source.Cell - 1u);
  }
  return result.Valid() ? std::optional(result) : std::nullopt;
}

bool BuildingSourceMetadata::Valid() const noexcept {
  if (!std::ranges::all_of(AnchorEcef, [](double value) { return std::isfinite(value); })) {
    return false;
  }
  uint64_t occupied = 0;
  for (size_t cell = 0; cell < CellSourceCounts.size(); ++cell) {
    if (CellSourceCounts[cell] != 0) { occupied |= uint64_t{1} << cell; }
  }
  return occupied == Cells;
}

std::optional<std::vector<uint8_t>>
EncodeBuildingSourceMetadata(const BuildingSourceMetadata &metadata) {
  BinaryValueWriter out(BuildingSourceMetadata::EncodedBytes);
  if (!metadata.Valid() || !out(kFormat,
                                metadata.AnchorEcef.Axis,
                                metadata.Cells,
                                metadata.HeightQualified,
                                metadata.CellSourceCounts)) {
    return std::nullopt;
  }
  return std::move(out.Out).TakeBytes();
}

std::optional<BuildingSourceMetadata> DecodeBuildingSourceMetadata(std::span<const uint8_t> bytes) {
  if (bytes.size() != BuildingSourceMetadata::EncodedBytes) { return std::nullopt; }
  BinaryValueReader in(bytes);
  BuildingSourceMetadata metadata;
  uint64_t format = 0;
  if (!in(format,
          metadata.AnchorEcef.Axis,
          metadata.Cells,
          metadata.HeightQualified,
          metadata.CellSourceCounts) ||
      format != kFormat || in.In.Remaining() != 0 || !metadata.Valid()) {
    return std::nullopt;
  }
  return metadata;
}
}
