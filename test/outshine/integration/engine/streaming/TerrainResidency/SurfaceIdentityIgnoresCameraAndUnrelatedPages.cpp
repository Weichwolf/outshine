#include "TerrainPublicationFixture.h"
#include "TerrainLattice.h"
#include "Check.h"
#include "RuntimeScene.h"
#include "SceneRenderer.h"
#include "streaming/TerrainResidency.h"
#include <SDL3/SDL.h>
#include <algorithm>
#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace {
constexpr auto kReplacementHeightM = 101;
constexpr auto kChildEnd = 2050;
constexpr auto kOutsideColumn = 2055;
constexpr auto kOutsideHeightM = 99;

using namespace outshine;
using namespace outshine::Test;
constexpr Data::TileId kParent{.Zoom = 11, .X = 1024, .Y = 1024};
constexpr Data::TileId kFocus{.Zoom = 12, .X = 2048, .Y = 2048};

Sheet Page(Data::TileId tile, float height) {
  return {.Tile = tile,
          .Nodes = std::vector<float>(Render::TerrainLattice::kPageNodes, height),
          .Side = Render::TerrainLattice::kSide,
          .Postings = Render::TerrainLattice::kSide};
}

bool CompletePublication(TerrainResidency &terrain,
                         const Patchwork &patch,
                         const TangentFrame &frame,
                         std::string &error) {
  const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(2);
  while (std::chrono::steady_clock::now() < deadline) {
    const auto ready = terrain.AdvancePublish(patch, frame, Render::kHeightPageBatchMost);
    if (!ready) {
      error = ready.error();
      return false;
    }
    if (*ready) { return true; }
    SDL_Delay(1);
  }
  error = "native publication did not finish within two seconds";
  return false;
}

void CheckIdentity(Render::SceneRenderer &renderer) {
  TerrainResidency terrain;
  terrain.Into(&renderer);
  auto key = terrain.SurfaceIdentity(kFocus, 2);
  CHECK(!key && key.error() == TerrainIdentityError::Unpublished,
        "unpublished heights cannot authorize a cached asset");
  Patchwork patch;
  patch.Sheets.push_back(Page(kParent, 1));
  for (uint32_t x = 2048; x < kChildEnd; ++x) {
    for (uint32_t y = 2048; y < kChildEnd; ++y) {
      patch.Sheets.push_back(Page({.Zoom = 12, .X = x, .Y = y}, static_cast<float>(x + y)));
    }
  }
  patch.Sheets.push_back(Page({.Zoom = 12, .X = kOutsideColumn, .Y = 2048}, kOutsideHeightM));
  const auto frame = TangentFrame::At({});
  std::string error;
  CHECK(PublishTerrain(terrain, patch, frame, error), "the complete terrain publication succeeds");
  const auto initial = terrain.SurfaceIdentity(kFocus, 2);
  const auto parent = terrain.SurfaceIdentity(kParent, 5);
  CHECK(initial && parent, "ancestor and descendant pages participate in the local source set");
  CHECK(terrain.SurfaceIdentity(kFocus, 1) == initial,
        "a covering detailed page excludes coarser ancestors that cannot supply this region");
  CHECK(terrain.SurfaceIdentity({.Zoom = 10, .X = 512, .Y = 512}, 5) == parent,
        "scanning sparse descendants and enumerating dense tile cells select the same sources");
  key = terrain.SurfaceIdentity({.Zoom = 12, .X = 4096, .Y = 2048}, 6);
  CHECK(!key && key.error() == TerrainIdentityError::InvalidRequest,
        "invalid geographic addresses cannot become source identities");
  key = terrain.SurfaceIdentity(kParent, 4);
  CHECK(!key && key.error() == TerrainIdentityError::OverBudget,
        "a short source budget never returns a partial identity");
  key = terrain.SurfaceIdentity({.Zoom = 2, .X = 0, .Y = 0}, 6);
  CHECK(!key && key.error() == TerrainIdentityError::NoSource,
        "a valid geographic region without resident input stays unknown");
  const uint64_t oldDiagnostic = terrain.Digest();
  const auto moved = TangentFrame::At({.LongitudeDeg = 2, .LatitudeDeg = 1});
  std::ranges::reverse(patch.Sheets);
  CHECK(PublishTerrain(terrain, patch, moved, error),
        "a reordered publication can rebase its render frame");
  CHECK(terrain.SurfaceIdentity(kFocus, 2) == initial &&
            terrain.SurfaceIdentity(kParent, 5) == parent,
        "geographic content identity ignores publication order and render origin");
  CHECK(terrain.Digest() != oldDiagnostic, "the former diagnostic still observes render placement");
  patch.Sheets.front().Nodes.assign(Render::TerrainLattice::kPageNodes, kReplacementHeightM);
  CHECK(PublishTerrain(terrain, patch, moved, error),
        "an unrelated geographic page can be replaced");
  CHECK(terrain.SurfaceIdentity(kFocus, 2) == initial &&
            terrain.SurfaceIdentity(kParent, 5) == parent,
        "outside content does not invalidate a local asset");
  auto &changed = patch.Sheets[patch.Sheets.size() - 2];
  CHECK(changed.Tile == kFocus, "the independent fixture changes the selected child");
  changed.Nodes.assign(Render::TerrainLattice::kPageNodes, 7);
  CHECK(PublishTerrain(terrain, patch, moved, error), "a selected deformed page can be replaced");
  CHECK(terrain.SurfaceIdentity(kFocus, 2) != initial &&
            terrain.SurfaceIdentity(kParent, 5) != parent,
        "actual local height changes invalidate child and containing-region identities");
  const auto changedKey = terrain.SurfaceIdentity(kFocus, 2);
  changed.Postings += 1;
  CHECK(terrain.BeginPublish(patch, error) && CompletePublication(terrain, patch, moved, error),
        error.c_str());
  CHECK(terrain.SurfaceIdentity(kFocus, 2) != changedKey,
        "sampling layout belongs to the content dependency even for reused GPU height storage");
  const TerrainResidency snapshot(terrain);
  CHECK(snapshot.SurfaceIdentity(kFocus, 2) == terrain.SurfaceIdentity(kFocus, 2),
        "an immutable query snapshot preserves metadata indexing");
  CHECK(terrain.BeginPublish(patch, error), "an incremental replacement can begin");
  key = terrain.SurfaceIdentity(kFocus, 2);
  CHECK(!key && key.error() == TerrainIdentityError::Unpublished,
        "a partially changed surface cannot be recorded as a complete cache dependency");
  const TerrainResidency unfinished(terrain);
  CHECK(!unfinished.SurfaceIdentity(kFocus, 2), "copying does not publish an unfinished surface");
  terrain.Clear();
}
}

int main() {
  CHECK(SDL_Init(SDL_INIT_VIDEO), "video initializes");
  {
    Core::Declaration declaration;
    declaration.SurfaceWidthPx = declaration.SurfaceHeightPx = 32;
    declaration.Outputs = {"surface"};
    Render::SceneRenderer renderer;
    std::unique_ptr<Core::RuntimeScene> scene;
    std::string error;
    CHECK(Core::RuntimeScene::Open(renderer, declaration, nullptr, scene, error), "world opens");
    if (scene) { CheckIdentity(renderer); }
  }
  SDL_Quit();
  return Report();
}
