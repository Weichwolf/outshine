#include "PreparedBuildingAssets.h"
#include "PreparedStructureTile.h"
#include "ClusterCook.h"
#include "Check.h"
#include "OpenAssetStore.h"
#include "RawSourceCache.h"
#include "SourceSet.h"
#include <chrono>
#include <filesystem>
#include <string>
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
  PreparedStructureTile source;
  BakedTile shell;
  shell.RequestedCell = request.RequestedCell;
  shell.RequestedDetail = request.RequestedDetail;
  shell.OccupiedCells = 1;
  shell.SurfaceError.emplace();
  const Vec3f normal{{0, 0, 1}};
  const std::vector<Vec3f> positions{{{0, 0, 0}}, {{10, 0, 0}}, {{10, 5, 0}}};
  for (const auto &position : positions) {
    shell.Built.WallCorners.push_back(StoredVertex::Of(position, {{0.5f, 0.25f}}, normal));
  }
  shell.Built.WallRun = {2, 0, 1};
  const auto write = (*assets)->PrepareGeometry(parent, source, request, shell);
  CHECK(write && (*assets)->PublishProduct(*write), "certified opaque geometry is available");
  if (!write) { return; }

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
