#include "PreparedBuildingAssets.h"
#include "PreparedStructureTile.h"
#include "ClusterCook.h"
#include "Check.h"
#include "OpenAssetStore.h"
#include "RawSourceCache.h"
#include "SourceSet.h"
#include "StructureArtifact.h"
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <span>
#include <string>
#include <system_error>
#include <vector>

namespace {
using namespace outshine;
using namespace outshine::Generators;
using namespace outshine::Test;

void CheckShadowCell(const std::filesystem::path &root) {
  const auto cache = OpenAssetStore(root.string());
  Data::RawSourceCache raw({.Directory = (root / "sources").string()});
  const Data::SourceSet sources(raw);
  const auto assets = PreparedBuildingAssets::Open(cache, sources);
  CHECK(assets, "building store opens");
  if (!assets) { return; }

  RawTile request;
  request.RequestedDetail = LevelOfDetail::Shell;
  request.RequestedCell = 1;
  request.ClusterTriangles = kClusterTriangles;
  const auto parent = (*assets)->Key({.Zoom = 0, .X = 0, .Y = 0}, 0, {}, 1000);
  const PreparedStructureTile source;
  BakedTile shell;
  shell.RequestedCell = request.RequestedCell;
  shell.RequestedDetail = request.RequestedDetail;
  shell.OccupiedCells = 1;
  constexpr uint64_t sourceKey = 1;
  shell.SurfaceError.emplace();
  shell.SurfaceError->Upper = {.SourceKey = sourceKey, .ReferenceToVariantM = 1};
  const Vec3f normal{{0, 0, 1}};
  const std::vector<Vec3f> positions{{{0, 0, 0}}, {{10, 0, 0}}, {{10, 5, 0}}};
  for (const auto &position : positions) {
    shell.Built.WallCorners.push_back(StoredVertex::Of(position, {{0.5f, 0.25f}}, normal));
  }
  shell.Built.WallRun = {2, 0, 1};
  const auto write = (*assets)->PrepareGeometry(parent, source, request, shell);
  CHECK(write, "the opaque geometry is serializable");
  if (!write) { return; }
  const auto archive = EncodeStructureArtifact(shell, write->Key);
  if (!archive) {
    CHECK(false, "the legacy archive can be written");
    return;
  }
  const AssetRecord unindexed{.Key = write->Key,
                              .Kind = "building-static-lod",
                              .Bounds = shell.BoundsAt(source.AnchorEcef),
                              .Package = {},
                              .ByteCount = archive->size(),
                              .Level = static_cast<uint32_t>(LevelOfDetail::Shell),
                              .Parent = parent};
  CHECK(cache->Publish(std::span(&unindexed, 1), *archive),
        "an old product exists without tile metadata");
  const auto loaded = (*assets)->LoadGeometry(parent, source, request, sourceKey);
  const auto indexed = cache->FindTile(write->Key);
  CHECK(loaded && *loaded && indexed && *indexed && (**indexed).Tile &&
            (**indexed).Tile->ErrorM == 1.0 && (**indexed).Tile->Revision == parent,
        "loading the old product publishes its error certificate without replacing its body");
  constexpr double kFocalPx = 720;
  constexpr double kCloseM = 20;
  constexpr double kFarM = 10000;
  request.Projection = {.FocalPx = kFocalPx, .AllowedErrorPx = 1};
  request.EyeEcef = Vec3{{0, 0, kCloseM}};
  CHECK((*assets)->RejectsCachedShell(parent, request, sourceKey),
        "a near camera rejects a cached shell using spatial metadata");
  request.EyeEcef = Vec3{{0, 0, kFarM}};
  CHECK(!(*assets)->RejectsCachedShell(parent, request, sourceKey),
        "a distant camera can request the shell when its projected error fits");

  BuildingShadowCell::ReadCost cost;
  const auto initial = (*assets)->LoadShadowCell(parent, 1, cost);
  CHECK(initial, "the exact opaque shadow geometry is prepared on demand");
  if (!initial) { return; }
  CHECK(cache->Remove(write->Key), "the material geometry may expire independently");
  const auto hit = (*assets)->LoadShadowCell(parent, 1, cost);
  CHECK(hit && hit->Parts[0].Indices == shell.Built.WallRun &&
            hit->Parts[0].Vertices.size() == positions.size(),
        "the position-only asset retains the original winding without material geometry");
  if (!hit || hit->Parts[0].Vertices.size() != positions.size()) { return; }
  for (size_t at = 0; at < positions.size(); ++at) {
    CHECK(hit->Parts[0].Vertices[at].pos == positions[at], "the caster positions are preserved");
  }
  CHECK(!(*assets)->LoadShadowCell(parent, 2, cost),
        "the caster cannot stand in for geometry in another cell");
}
}

int main() {
  const auto root = std::filesystem::temp_directory_path() /
                    ("outshine-shadow-cell-" +
                     std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
  CheckShadowCell(root);
  std::error_code error;
  std::filesystem::remove_all(root, error);
  return outshine::Test::Report();
}
