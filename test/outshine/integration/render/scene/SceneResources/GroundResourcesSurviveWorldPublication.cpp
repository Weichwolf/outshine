#include "TerrainLattice.h"
#include "RuntimeScene.h"
#include "WorldCandidate.h"
#include "EngineState.h"
#include "SceneRenderer.h"
#include "FrameReadback.h"
#include "Check.h"
#include "test/harness/render/NativeUploadFaultDevice.h"
#include <SDL3/SDL.h>
#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace {
outshine::Geometry Triangle() {
  using namespace outshine;
  Geometry geometry;
  const int part = geometry.addPart("triangle", MaterialInstance{}).value();
  CHECK(geometry.setPositions(part, std::array<float, 9>{-1, -1, 0, 1, -1, 0, 0, 1, 0}) &&
            geometry.setTriangles(part, std::array<uint32_t, 3>{0, 1, 2}),
        "replacement geometry is valid");
  return geometry;
}

outshine::Render::Viewpoint GroundView() {
  constexpr double kNearM = 0.1;
  constexpr double kFarM = 100;
  auto eye = outshine::Render::Viewpoint::LookAt({.EyeM = {{0, 7, 8}}, .AimM = {{0, 3, 0}}}, 0.0);
  CHECK(eye.has_value(), "the original ground-only camera remains valid");
  if (!eye) { return {}; }
  eye->YfovRad = 1;
  eye->ZNearM = kNearM;
  eye->ZFarM = kFarM;
  return *eye;
}
}

int main() {
  using namespace outshine;
  using namespace outshine::Test;
  CHECK(SDL_Init(SDL_INIT_VIDEO), "video initializes");
  {
    const Geometry geometry = Triangle();
    Core::Declaration declaration;
    declaration.SurfaceWidthPx = declaration.SurfaceHeightPx = 32;
    declaration.Outputs = {"surface"};
    Gpu::UploadFailure failure;
    Render::SceneRenderer renderer(
        {.Context = &failure, .Create = Gpu::NativeUploadFaultDevice::Create});
    const auto uploads = renderer.OfflineUploads(std::chrono::seconds(5));
    std::unique_ptr<Core::RuntimeScene> scene;
    std::string error;
    CHECK(Core::RuntimeScene::Open(renderer, declaration, nullptr, scene, error),
          "ground fixture opens");
    if (scene) {
      WorldRegionState world;
      world.BindSceneResources(renderer);
      std::vector<float> nodes(Render::TerrainLattice::kPageNodes, 3.0f);
      const auto removed = renderer.PlaceHeightPage(nodes);
      CHECK(removed.has_value(), "a predecessor creates a gap in native slot identity");
      if (!removed) { return Report(); }
      const auto page = renderer.PlaceHeightPage(nodes);
      CHECK(page.has_value(), "a stable height-page handle is placed");
      if (!page) { return Report(); }
      renderer.ReleaseHeightPage(*removed);
      std::vector<float> fractions(Render::TerrainLattice::kSide);
      for (size_t at = 0; at < fractions.size(); ++at) {
        fractions[at] = static_cast<float>(at) / static_cast<float>(fractions.size() - 1u);
      }
      Render::TerrainTile tile;
      tile.Corners = {{-1, -1, 1, -1, -1, 1, 1, 1}};
      tile.Page = *page;
      tile.LowM = 3.0f;
      tile.HighM = 3.0f;
      CHECK(renderer.SetGroundGrid(fractions, error) &&
                renderer.SetTerrainTiles({&tile, 1}, {}, error),
            "ground inputs retain stable page handles");
      CHECK(renderer.TerrainRendererTriangles() == Render::TerrainLattice::kIndices / 3u,
            "one published ground tile has its full topology");
      scene->Eye(GroundView());
      CHECK(scene->Draw(error), "a ground-only frame renders without any object placement buffer");
      CHECK(renderer.TerrainRendererTriangles() > 0,
            "the ground-only draw actually culls the test tile into the frame");
      std::vector<uint8_t> pixels;
      CHECK(Render::ReadFrame(renderer, pixels, error) && pixels.size() == size_t{32} * 32u * 4u,
            "the ground-only frame is readable at the declared size");
      std::unique_ptr<Core::RuntimeScene> candidate;
      const auto pipelines = failure.ComputePipelineRequests;
      CHECK(pipelines >= 4, "the resident frame owns actual terrain selection pipelines");
      CHECK(Core::RuntimeScene::PreparesWorldReplacement(
                renderer, *scene, nullptr, candidate, error) &&
                Core::RuntimeScene::PublishesPreparedWorld(renderer, scene, candidate, error),
            "an empty declared world publishes its first streamed-ground candidate");
      CHECK(failure.ComputePipelineRequests == pipelines,
            "a new world candidate binds resident frame pipelines without shader recompilation");
      world.BindSceneResources(renderer);
      CHECK(renderer.TerrainRendererTriangles() == Render::TerrainLattice::kIndices / 3u,
            "first streamed-world publication retains the ground tile topology");
      CHECK(Core::RuntimeScene::ReplacesGeometry(
                renderer, *scene, geometry.clone(), nullptr, scene, error),
            "geometry replacement recreates ground resources in its candidate");
      world.BindSceneResources(renderer);
      CHECK(renderer.TerrainRendererTriangles() == Render::TerrainLattice::kIndices / 3u,
            "candidate publication retains the ground tile topology");
      renderer.ReleaseHeightPage(*page);
      const auto replacement = renderer.PlaceHeightPage(nodes);
      CHECK(replacement && replacement->Slot == page->Slot &&
                replacement->Generation != page->Generation,
            "a replaced height page uses a new generation in the same native slot");
      if (!replacement) { return Report(); }
      Render::TerrainTile next = tile;
      next.Page = *replacement;
      CHECK(renderer.SetTerrainTiles({&next, 1}, {}, error), "replacement page publishes");
      CHECK(!renderer.SetTerrainTiles({&tile, 1}, {}, error) &&
                renderer.TerrainRendererTriangles() == Render::TerrainLattice::kIndices / 3u,
            "stale lattice input is rejected without changing the current topology");
      for (const Render::HeightPageHandle invalid :
           {Render::HeightPageHandle{},
            Render::HeightPageHandle{.Slot = Render::kNoResourceSlot - 1, .Generation = 1},
            Render::HeightPageHandle{.Slot = replacement->Slot, .Generation = 0}}) {
        Render::TerrainTile refused = next;
        refused.Page = invalid;
        CHECK(!renderer.SetTerrainTiles({&refused, 1}, {}, error) &&
                  renderer.TerrainRendererTriangles() == Render::TerrainLattice::kIndices / 3u,
              "invalid page identities preserve the published lattice");
        renderer.ReleaseHeightPage(invalid);
      }
      const auto payload = renderer.HeightPageSourceBytes();
      renderer.ReleaseHeightPage(*page);
      CHECK(renderer.HeightPageSourceBytes() == payload,
            "stale page release cannot remove its successor");
      {
        Core::WorldCandidate rejected(renderer);
        const auto prepared = rejected.Prepare(*scene, nullptr);
        CHECK(prepared && renderer.SetTerrainTiles({&next, 1}, {}, error),
              "candidate reconstruction preserves the new page generation");
      }
      CHECK(renderer.SetTerrainTiles({&next, 1}, {}, error),
            "candidate rejection keeps published page identity usable");
      Core::WorldCandidate successor(renderer);
      CHECK(successor.Prepare(*scene, nullptr) && successor.Publish(scene) &&
                renderer.SetTerrainTiles({&next, 1}, {}, error),
            "a second candidate publishes the reused native generation");
      world.BindSceneResources(renderer);
      world.TerrainSurface.Clear();
      CHECK(renderer.TerrainRendererTriangles() == 0,
            "rebound streaming owner clears the current world after two replacements");
    }
  }
  SDL_Quit();
  return Report();
}
