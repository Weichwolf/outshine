#include "BuildingSourceMetadata.h"
#include "PreparedBuildingBasis.h"
#include "Check.h"
#include <cstdint>
#include <limits>
#include <vector>

namespace {
using namespace outshine;
using namespace outshine::Generators;
using namespace outshine::Test;

void Exercise() {
  PreparedBuildingBasis basis;
  basis.AnchorEcef = {{1, 2, 3}};
  basis.HeightQualified = true;
  basis.Sources = {{.Cell = 1, .Id = {}}, {.Cell = 64, .Id = {}}, {.Cell = 64, .Id = {}}};
  const auto metadata = BuildingSourceMetadata::Of(basis);
  CHECK(metadata && metadata->Cells == (uint64_t{1} | (uint64_t{1} << 63u)) &&
            metadata->CellSourceCounts[0] == 1 && metadata->CellSourceCounts[63] == 2,
        "the first and last cells retain their full source multiplicity");
  if (!metadata) { return; }
  const auto encoded = EncodeBuildingSourceMetadata(*metadata);
  CHECK(encoded && encoded->size() == 297 && (*encoded)[40] == 1 && (*encoded)[41] == 1 &&
            (*encoded)[293] == 2,
        "the versioned little-endian summary is exactly 297 bytes without coordinate arrays");
  if (!encoded) { return; }
  const auto decoded = DecodeBuildingSourceMetadata(*encoded);
  CHECK(decoded && decoded->AnchorEcef == basis.AnchorEcef && decoded->HeightQualified &&
            decoded->Cells == metadata->Cells &&
            decoded->CellSourceCounts == metadata->CellSourceCounts,
        "source selection receives the original origin, qualification and every count");
  for (const auto offset : {0, 32, 40, 41}) {
    auto corrupt = *encoded;
    corrupt[offset] = offset == 40 ? 2 : 0;
    if (offset == 0) { corrupt[offset] ^= 1; }
    CHECK(!DecodeBuildingSourceMetadata(corrupt),
          "wrong version, occupancy, boolean or source count cannot become valid metadata");
  }
  auto truncated = *encoded;
  truncated.pop_back();
  CHECK(!DecodeBuildingSourceMetadata(truncated), "truncation cannot be accepted as an empty cell");
  auto trailing = *encoded;
  trailing.push_back(0);
  CHECK(!DecodeBuildingSourceMetadata(trailing), "unversioned trailing data is rejected");
  for (const auto cell : {0u, 65u}) {
    basis.Sources.front().Cell = cell;
    CHECK(!BuildingSourceMetadata::Of(basis), "invalid source cells are not silently discarded");
  }
  auto invalid = *metadata;
  invalid.AnchorEcef[0] = std::numeric_limits<double>::quiet_NaN();
  CHECK(!EncodeBuildingSourceMetadata(invalid), "nonfinite anchors cannot reach the asset DB");
  basis.Sources.clear();
  const auto empty = BuildingSourceMetadata::Of(basis);
  CHECK(empty && empty->Cells == 0 && EncodeBuildingSourceMetadata(*empty),
        "a known-empty source remains a qualified explicit summary");
}
}

int main() {
  Exercise();
  return Report();
}
