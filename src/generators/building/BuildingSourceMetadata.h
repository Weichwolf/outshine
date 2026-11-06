#ifndef OUTSHINE_GENERATORS_BUILDING_BUILDINGSOURCEMETADATA_H
#define OUTSHINE_GENERATORS_BUILDING_BUILDINGSOURCEMETADATA_H

#include "Vec3.h"
#include "StructureCell.h"
#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <vector>

namespace outshine::Generators {
struct PreparedBuildingBasis;

struct BuildingSourceMetadata {
  Vec3 AnchorEcef;
  uint64_t Cells = 0;
  bool HeightQualified = false;
  std::array<uint32_t, kStructureCellsPerTile> CellSourceCounts{};

  static constexpr size_t EncodedBytes = sizeof(uint64_t) + 3 * sizeof(double) + sizeof(uint64_t) +
                                         1 + kStructureCellsPerTile * sizeof(uint32_t);

  [[nodiscard]] static std::optional<BuildingSourceMetadata> Of(const PreparedBuildingBasis &basis);
  [[nodiscard]] bool Valid() const noexcept;
};

[[nodiscard]] std::optional<std::vector<uint8_t>>
EncodeBuildingSourceMetadata(const BuildingSourceMetadata &metadata);
[[nodiscard]] std::optional<BuildingSourceMetadata>
DecodeBuildingSourceMetadata(std::span<const uint8_t> bytes);
}
#endif
