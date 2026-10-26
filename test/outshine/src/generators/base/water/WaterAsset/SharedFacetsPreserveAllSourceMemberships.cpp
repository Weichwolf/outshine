#include "WaterAsset.h"
#include "WaterSurfaceMesh.h"
#include "Check.h"
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <fstream>
#include <ios>
#include <iterator>
#include <utility>
#include <vector>

namespace {
void CheckLegacyRecord() {
  using namespace outshine;
  using namespace outshine::Generators;
  using namespace outshine::Test;
  std::ifstream file("test/outshine/data/water/prepared-v4.native", std::ios::binary);
  const std::vector<uint8_t> bytes{std::istreambuf_iterator<char>(file), {}};
  CHECK(!bytes.empty() && !file.bad(), "the fixed c7a24a365 WATER4 producer record is available");
  const auto old = WaterAsset::DecodeNative(bytes);
  CHECK(old && old->SurfacesReady(), "previously cached prepared water remains readable");
  if (!old) { return; }
  CHECK(old->PreparedSurfaces()->FaceSourceOffsets.empty() &&
            old->PreparedSurfaces()->FaceSources.empty(),
        "legacy facets retain their original single source without invented memberships");
  const auto encoded = old->EncodeNative(16384);
  CHECK(encoded && *encoded == bytes, "legacy assets retain their exact native bytes");
}
}

int main() {
  using namespace outshine;
  using namespace outshine::Generators;
  using namespace outshine::Test;
  CheckLegacyRecord();
  WaterAsset source(
      {.Surfaces = {{.FirstRing = 0, .RingCount = 1, .LevelM = 999},
                    {.FirstRing = 1, .RingCount = 1, .LevelM = 888}},
       .Rings = {{.FirstPoint = 0, .PointCount = 3}, {.FirstPoint = 0, .PointCount = 3}},
       .Courses = {},
       .Levels = {},
       .Points = {-.001, -.001, -.001, .001, .001, 0},
       .Tiles = {{.Zoom = 0, .X = 0, .Y = 0, .FirstSurface = 0, .SurfaceCount = 2}},
       .ProcessedTiles = 1,
       .Sources = {},
       .Inputs = {}});
  const auto mesh =
      WaterSurfaceMesh::Build({},
                              {{{-100, -100, 1}}, {{100, -100, 1.2}}, {{0, 100, 1.1}}},
                              {{.Vertices = {0, 1, 2}, .Surface = 0}});
  CHECK(mesh, "one continuous physical facet can cover overlapping source surfaces");
  if (!mesh) { return Report(); }
  const WaterAsset::SurfaceGeometry geometry{.Mesh = *mesh,
                                             .Rejections = {},
                                             .FaceSourceOffsets = {0, 2},
                                             .FaceSources = {0, 1},
                                             .ProfileDeclines = {}};
  const auto prepared = source.WithSurfaces(geometry);
  CHECK(prepared, "both original surfaces are covered without duplicating physical facets");
  if (!prepared) { return Report(); }
  CHECK(prepared->PreparedSurfaces()->Mesh.Faces().size() == 1 &&
            prepared->PreparedSurfaces()->Rejections.empty(),
        "shared coverage is not represented as an omission or duplicated geometry");
  const auto encoded = prepared->EncodeNative(16384);
  CHECK(encoded, "the complete source membership relation fits the native asset");
  if (!encoded) { return Report(); }
  const auto restored = WaterAsset::DecodeNative(*encoded);
  CHECK(restored && restored->PreparedSurfaces()->FaceSourceOffsets == geometry.FaceSourceOffsets &&
            restored->PreparedSurfaces()->FaceSources == geometry.FaceSources &&
            restored->LevelAt({}) == prepared->LevelAt({}),
        "native replay preserves every source and the same physical contact");
  CHECK(prepared->PreparedSurfaceBytes() >=
            sizeof(WaterAsset::SurfaceGeometry) + mesh->HeapBytes() + 4 * sizeof(uint32_t),
        "owned sparse source arrays are included in prepared memory accounting");
  const auto reject = [&](auto mutate) {
    auto changed = geometry;
    mutate(changed);
    CHECK(!source.WithSurfaces(std::move(changed)), "invalid source membership is rejected");
  };
  reject([](auto &g) { g.FaceSourceOffsets = {0}; });
  reject([](auto &g) { g.FaceSourceOffsets = {1, 2}; });
  reject([](auto &g) { g.FaceSourceOffsets = {0, 3}; });
  reject([](auto &g) {
    g.FaceSourceOffsets = {0, 0};
    g.FaceSources.clear();
  });
  reject([](auto &g) { g.FaceSourceOffsets.clear(); });
  reject([](auto &g) { g.FaceSources = {0, 0}; });
  reject([](auto &g) { g.FaceSources = {1, 0}; });
  reject([](auto &g) { g.FaceSources = {0, 2}; });
  reject([](auto &g) {
    g.FaceSourceOffsets = {0, 1};
    g.FaceSources = {1};
  });
  reject([](auto &g) { g.Rejections = {{.Surface = 1}}; });
  auto corrupt = *encoded;
  std::fill(corrupt.end() - static_cast<std::ptrdiff_t>(sizeof(uint32_t)), corrupt.end(), 255);
  CHECK(!WaterAsset::DecodeNative(corrupt), "persisted unknown source IDs fail admission");
  return Report();
}
