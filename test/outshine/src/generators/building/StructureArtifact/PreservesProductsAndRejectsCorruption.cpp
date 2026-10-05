#include "StructureArtifact.h"
#include "BuildingMesh.h"
#include "Sha256.h"
#include "Check.h"
#include <algorithm>
#include <limits>

int main() {
  using namespace outshine;
  using namespace outshine::Generators;
  using namespace outshine::Test;
  RawTile raw;
  raw.LatLon = {47.0, 9.0, 47.0, 9.0001, 47.0001, 9.0001, 47.0001, 9.0};
  const Ground::GeoBounds bounds{
      .MinLonDeg = 9.0, .MinLatDeg = 47.0, .MaxLonDeg = 9.008, .MaxLatDeg = 47.008};
  const auto cell = StructureCellOf(bounds, raw.LatLon);
  CHECK(cell.has_value(), "fixture has a valid cell");
  if (!cell) { return Report(); }
  raw.Structures.push_back({.PointCount = 4, .SourceFirst = 12, .Cell = *cell, .HeightM = 12.0});
  raw.Structures.front().WallColour = Vec3f{{0.2f, 0.4f, 0.6f}};
  raw.RequestedDetail = LevelOfDetail::Shell;
  raw.RequestedCell = cell->Index;
  raw.TileSpanM = 1000.0;
  Ground::HeightField::Block block;
  block.At = {.Zoom = 0, .X = 0, .Y = 0};
  block.Raster = {.Side = 2, .Postings = 2};
  block.Nodes.assign(4, 0.0f);
  const auto heights = Ground::HeightField::Of(0, {block});
  BuildingMesh mesher;
  auto scratch = mesher.Scratch();
  BakedTile product;
  CHECK(BakeStructures(raw, *heights, mesher, *scratch, product).has_value(),
        "real building producer supplies the artifact");
  product.SurfaceError = StructureSurfaceErrorInterval{
      .Upper = {.SourceKey = 17, .ReferenceToVariantM = 0.2, .VariantToReferenceM = 0.3},
      .ReferenceToVariantLowerM = 0.1,
      .VariantToReferenceLowerM = 0.1};
  const auto key = Sha256Hex("complete-input-manifest");
  const auto encoded = EncodeStructureArtifact(product, key);
  CHECK(encoded.has_value(), "complete product encodes");
  if (!encoded) { return Report(); }
  auto decoded = DecodeStructureArtifact(*encoded, key, 29);
  CHECK(decoded.has_value(), "product decodes under current source identity");
  if (!decoded) { return Report(); }
  CHECK(decoded->SurfaceError && decoded->SurfaceError->Upper.SourceKey == 29 &&
            decoded->SurfaceError->Upper.ReferenceToVariantM == 0.2 &&
            decoded->SurfaceError->Upper.VariantToReferenceM == 0.3,
        "error distances survive while runtime identity is rebound");
  CHECK(decoded->Prints == product.Prints &&
            decoded->FootprintDetails == product.FootprintDetails &&
            decoded->CellBounds == product.CellBounds &&
            decoded->CellMaxHeightM == product.CellMaxHeightM &&
            decoded->CellShellErrorM == product.CellShellErrorM &&
            decoded->OccupiedCells == product.OccupiedCells && decoded->Digest == product.Digest &&
            decoded->Walls.Clusters == product.Walls.Clusters &&
            decoded->Walls.Index == product.Walls.Index &&
            decoded->Roofs.Clusters == product.Roofs.Clusters &&
            decoded->Roofs.Index == product.Roofs.Index &&
            decoded->Built.WallRun == product.Built.WallRun && !product.Built.WallColours.empty() &&
            decoded->Built.WallColours == product.Built.WallColours &&
            decoded->Built.RoofRun == product.Built.RoofRun,
        "semantic, geometric and clustered products remain intact");
  CHECK(EncodeStructureArtifact(*decoded, key) == encoded,
        "canonical artifact bytes do not depend on runtime source identity");
  CHECK(!DecodeStructureArtifact(*encoded, Sha256Hex("changed-input"), 29),
        "changed input cannot reuse this product");
  CHECK(!DecodeStructureArtifact(*encoded, key, 0), "proof needs current source identity");
  for (size_t size = 0; size < encoded->size(); ++size) {
    CHECK(!DecodeStructureArtifact(std::span(*encoded).first(size), key, 29),
          "every truncated artifact is rejected");
  }
  auto corrupt = *encoded;
  corrupt[corrupt.size() / 2] ^= 1;
  CHECK(!DecodeStructureArtifact(corrupt, key, 29), "payload corruption fails checksum");
  corrupt = *encoded;
  constexpr size_t firstVectorCount =
      std::string_view("outshine-structure").size() + sizeof(uint32_t) + 64;
  std::fill_n(corrupt.begin() + firstVectorCount, sizeof(uint64_t), uint8_t{255});
  const auto checksum = Sha256Hex(corrupt.data(), corrupt.size() - 64);
  std::copy(checksum.begin(), checksum.end(), corrupt.end() - 64);
  CHECK(!DecodeStructureArtifact(corrupt, key, 29),
        "valid checksum cannot bypass bounded allocation");
  product.Built.WallRun.push_back(std::numeric_limits<uint32_t>::max());
  CHECK(!EncodeStructureArtifact(product, key), "invalid geometry cannot be published");
  return Report();
}
