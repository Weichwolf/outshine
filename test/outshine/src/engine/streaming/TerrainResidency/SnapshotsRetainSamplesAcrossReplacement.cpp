#include "TerrainResidency.h"
#include "RuntimeScene.h"
#include "SceneRenderer.h"
#include "TerrainRenderer.h"
#include "Check.h"

#include <SDL3/SDL.h>
#include <cstddef>
#include <memory>
#include <string>
#include <vector>

int main() {
  using namespace outshine;
  using namespace outshine::Test;
  CHECK(SDL_Init(SDL_INIT_VIDEO), "video initializes");
  {
    Render::SceneRenderer renderer;
    Core::Declaration declaration;
    declaration.SurfaceWidthPx = declaration.SurfaceHeightPx = 32;
    declaration.Outputs = {"surface"};
    std::unique_ptr<Core::RuntimeScene> scene;
    std::string error;
    CHECK(Core::RuntimeScene::Open(renderer, declaration, nullptr, scene, error),
          "the native terrain consumer opens");
    if (scene) {
      constexpr Data::TileId tile{.Zoom = 12, .X = 2048, .Y = 2048};
      constexpr LongitudeLatitude point{.LongitudeDeg = 0.02, .LatitudeDeg = -0.02};
      const auto frame = TangentFrame::At({});
      Patchwork patch;
      patch.Sheets.push_back({.Tile = tile,
                              .Nodes = std::vector<float>(Render::TerrainRenderer::kPageNodes, 2),
                              .Side = Render::TerrainRenderer::kSide,
                              .Postings = Render::TerrainRenderer::kSide});
      TerrainResidency terrain;
      terrain.Into(&renderer);
      CHECK(terrain.Publish(patch, frame, error), error.c_str());
      const auto initial = terrain.SurfaceIdentity(tile, 1);
      const auto height = terrain.HeightMAt(point);
      CHECK(initial && height, "published samples provide both content identity and local height");
      const TerrainResidency snapshot(terrain);
      const size_t payload = patch.Sheets.front().Nodes.size() * sizeof(float);
      CHECK(snapshot.HeapBytes() >= snapshot.HeapBytes(TerrainHeapScope::Additional) + payload,
            "incremental snapshot accounting excludes the shared sample payload");
      CHECK(snapshot.SurfaceIdentity(tile, 1) == initial && snapshot.HeightMAt(point) == height,
            "a snapshot preserves exact content and interpolated height");
      {
        TerrainResidency pending(terrain);
        CHECK(pending.BeginPublish(patch, error), "a copied terrain begins a new publication");
        const TerrainResidency pendingSnapshot(pending);
        CHECK(pendingSnapshot.Standing() == pending.Standing() &&
                  pendingSnapshot.HeightMAt(point) == height &&
                  !pendingSnapshot.HasPublishedSurface(),
              "copying a pending publication preserves old queries and pending visibility");
      }
      patch.Sheets.front().Nodes.assign(Render::TerrainRenderer::kPageNodes, 9);
      CHECK(terrain.Publish(patch, frame, error), error.c_str());
      CHECK(terrain.SurfaceIdentity(tile, 1) != initial && terrain.HeightMAt(point) != height,
            "replacement publishes its own changed sample buffer");
      CHECK(snapshot.SurfaceIdentity(tile, 1) == initial && snapshot.HeightMAt(point) == height,
            "replacement cannot mutate the older immutable snapshot");
      terrain.Clear();
      const TerrainResidency emptySnapshot(terrain);
      CHECK(emptySnapshot.Standing() == 0 && !emptySnapshot.HeightMAt(point) &&
                emptySnapshot.HeapBytes() == 0,
            "copying cleared terrain does not retain obsolete index capacity");
      CHECK(snapshot.SurfaceIdentity(tile, 1) == initial && snapshot.HeightMAt(point) == height,
            "retiring the original publication leaves retained samples queryable");
      TerrainResidency nextSnapshot(snapshot);
      CHECK(nextSnapshot.SurfaceIdentity(tile, 1) == initial &&
                nextSnapshot.HeightMAt(point) == height &&
                nextSnapshot.HeapBytes() >=
                    nextSnapshot.HeapBytes(TerrainHeapScope::Additional) + payload,
            "a retained snapshot can supply another snapshot without copying sample data");
      nextSnapshot = TerrainResidency{};
      CHECK(nextSnapshot.Standing() == 0 && snapshot.SurfaceIdentity(tile, 1) == initial &&
                snapshot.HeightMAt(point) == height,
            "resetting the copied snapshot cannot change its retained source");
    }
  }
  SDL_Quit();
  return Report();
}
