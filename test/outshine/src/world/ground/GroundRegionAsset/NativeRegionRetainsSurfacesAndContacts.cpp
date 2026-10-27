#include "GroundRegionAsset.h"
#include "Check.h"
#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <limits>
#include <algorithm>
#include <utility>
#include <string>
#include <vector>

int main() {
  using namespace outshine;
  using namespace outshine::Test;
  constexpr size_t bytesMost = size_t{1024} * 1024;
  GroundRegionAsset region;
  region.Anchor = {.LongitudeDeg = 9, .LatitudeDeg = 47};
  region.GroundAlbedo = {{0.2, 0.3, 0.1}};
  region.MissingRims = 3;
  region.SurfaceManifest = std::string(64, 'a');
  region.Terrain.Sheets = {{.Tile = {.Zoom = 14, .X = 8601, .Y = 5761},
                            .Nodes = {100, 101, 102, 103},
                            .Side = 2,
                            .Postings = 2}};
  region.Terrain.Tiles = 1;
  const std::array<float, 9> positions{0, 100, 0, 10, 101, 0, 0, 102, 10};
  const std::array<uint32_t, 3> indices{0, 1, 2};
  Material pavement{};
  pavement.Roughness = 0.75f;
  auto surface = region.Surfaces.addSurface("pavement", pavement);
  CHECK(surface, "native material is created");
  if (!surface) { return Report(); }
  region.GroundSurface = surface->index();
  auto part = region.Surfaces.addPart("bridge", *surface);
  CHECK(part && region.Surfaces.setPositions(*part, positions) &&
            region.Surfaces.setTriangles(*part, indices),
        "native transport geometry is created");
  const auto grid = std::make_shared<TerrainClassificationMap::Grid>();
  grid->W = 1;
  grid->H = 1;
  grid->BoundaryHaloM = .75;
  grid->Cells = {7, 0};
  region.Classes = std::make_shared<const TerrainClassificationMap>(
      TangentFrame::At(region.Anchor),
      grid,
      grid,
      TerrainClassificationMap::FromRun{.Version = 123, .UnmappedRow = 255});
  region.ClassPalette = {0.25f, 0.5f, 0.75f};
  auto encoded = EncodeGroundRegionAsset(region, bytesMost);
  CHECK(encoded, "complete region encodes with canonical geometry");
  if (!encoded) { return Report(); }
  auto restored = DecodeGroundRegionAsset(*encoded, encoded->size());
  CHECK(restored, "region loads inside the exact payload bound");
  if (!restored) { return Report(); }
  CHECK(restored->Terrain.Sheets == region.Terrain.Sheets &&
            std::ranges::equal(restored->Surfaces.positionsOf(0), positions) &&
            std::ranges::equal(restored->Surfaces.trianglesOf(0), indices) &&
            restored->GroundAlbedo == region.GroundAlbedo && restored->MissingRims == 3 &&
            restored->ClassPalette == region.ClassPalette,
        "deformed contact pages, collision and material inputs survive");
  CHECK(restored->SurfaceManifest == region.SurfaceManifest,
        "the independent surface manifest reference survives without embedding its meshes");
  const auto reference = region.SurfaceManifest;
  region.SurfaceManifest = "invalid-reference";
  CHECK(!EncodeGroundRegionAsset(region, bytesMost),
        "invalid manifest identities cannot enter a region package");
  region.SurfaceManifest = reference;
  CHECK(restored->Classes && restored->Classes->Version() == 123 &&
            restored->Classes->Fine().BoundaryHaloM == .75 &&
            restored->Classes->Evaluate(0.5, 0.5, nullptr, nullptr) == 7 &&
            restored->Surfaces.surfaceAt(MaterialInstance(restored->GroundSurface)).Roughness ==
                0.75f,
        "classification and native surface ownership survive");
  CHECK(EncodeGroundRegionAsset(*restored, bytesMost) == encoded,
        "loaded product is byte-equivalent without source reconstruction");
  CHECK(GroundRegionBoundsEcef(region) == GroundRegionBoundsEcef(*restored),
        "spatial bounds are derived from the same native product");
  CHECK(!DecodeGroundRegionAsset(*encoded, encoded->size() - 1), "caller byte limit is enforced");
  encoded->pop_back();
  CHECK(!DecodeGroundRegionAsset(*encoded, bytesMost), "truncated native geometry is refused");
  region.Terrain.Sheets[0].Nodes[0] = std::numeric_limits<float>::quiet_NaN();
  CHECK(!EncodeGroundRegionAsset(region, bytesMost),
        "invalid contact height cannot enter the cache");
  region.Terrain.Sheets[0].Nodes[0] = 100;
  grid->Cells[0] |= 1u << 16u;
  CHECK(!EncodeGroundRegionAsset(region, bytesMost),
        "invalid classification seed cannot enter the cache");
  return Report();
}
