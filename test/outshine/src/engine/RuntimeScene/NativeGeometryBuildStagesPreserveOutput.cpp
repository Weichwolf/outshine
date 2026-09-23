#include "RuntimeScene.h"
#include "SceneRenderer.h"
#include "Check.h"
#include <SDL3/SDL.h>
#include <array>
#include <cstdint>
#include <memory>
#include <string>
#include <utility>
#include <vector>

int main() {
  using namespace outshine;
  using namespace outshine::Test;
  const bool initialized = SDL_Init(SDL_INIT_VIDEO);
  CHECK(initialized, "video initialized");
  if (!initialized) { return Report(); }
  {
    Geometry seed;
    const int part = seed.addPart("triangle", MaterialInstance{}).value();
    CHECK(seed.setPositions(part, std::array<float, 9>{-1, -1, 0, 1, -1, 0, 0, 1, 0}) &&
              seed.setTriangles(part, std::array<uint32_t, 3>{0, 1, 2}),
          "seed geometry is valid");
    Geometry target = seed.clone();
    CHECK(target.setPositions(part, std::array<float, 9>{-2, -1, 0, 2, -1, 0, 0, 2, 0}),
          "replacement geometry is valid");
    const int second = target.addPart("second", MaterialInstance{}).value();
    CHECK(
        target.setPositions(
            second,
            std::array<float, 9>{-0.5f, -0.5f, 0.1f, 0.5f, -0.5f, 0.1f, 0.0f, 0.5f, 0.1f}) &&
            target.setNormals(second, std::array<float, 9>{0, 0, 1, 0, 0, 1, 0, 0, 1}) &&
            target.setTangents(second, std::array<float, 12>{1, 0, 0, 1, 1, 0, 0, 1, 1, 0, 0, 1}) &&
            target.setTexture(second, std::array<float, 6>{0, 0, 1, 0, 0.5f, 1}) &&
            target.setColours(second, std::array<float, 12>{1, 0, 0, 1, 0, 1, 0, 1, 0, 0, 1, 1}) &&
            target.setTriangles(second, std::array<uint32_t, 3>{0, 1, 2}),
        "second part carries independent texture, frame and colour streams");
    Core::Declaration declaration;
    declaration.InitialGeometry = &seed;
    declaration.SurfaceWidthPx = declaration.SurfaceHeightPx = 32;
    declaration.Outputs = {"sceneLinear"};
    declaration.Surfacing.front().Unlit = true;
    Render::SceneRenderer directRenderer, pacedRenderer;
    std::unique_ptr<Core::RuntimeScene> direct, paced;
    std::string error;
    const bool directOpened =
        Core::RuntimeScene::Open(directRenderer, declaration, nullptr, direct, error);
    CHECK(directOpened, directOpened ? "direct scene opens" : error.c_str());
    const bool pacedOpened =
        Core::RuntimeScene::Open(pacedRenderer, declaration, nullptr, paced, error);
    CHECK(pacedOpened, pacedOpened ? "paced scene opens" : error.c_str());
    if (direct && paced) {
      direct->Digests(true);
      paced->Digests(true);
      const Material surface = declaration.Surfacing.front();
      CHECK(direct->SetGeometry(target.clone(), 1, surface, error),
            "direct geometry build completes");
      const auto began = paced->BeginGeometryBuild(std::move(target), 1, Material(surface));
      CHECK(began.has_value() && paced->GeometryBuildActive(), "paced geometry build starts");
      if (began) {
        const auto duplicate = paced->BeginGeometryBuild(seed.clone(), 1, Material(surface));
        CHECK(!duplicate && paced->GeometryBuildActive(),
              "a second build cannot replace an active candidate");
        bool complete = false;
        bool sawPendingUpload = false;
        size_t advances = 0;
        for (; advances < 1000 && !complete; ++advances) {
          auto advanced = paced->AdvanceGeometryBuild(1);
          CHECK(advanced.has_value(), advanced ? "paced stage advances" : advanced.error().c_str());
          if (!advanced) { break; }
          complete = *advanced;
          sawPendingUpload |= pacedRenderer.SubjectMeshPending();
          CHECK(paced->GeometryBuildActive() != complete,
                "build remains active exactly until finalization");
        }
        CHECK(complete && advances >= 5 && sawPendingUpload,
              "shape, planning, packing, index and stream phases complete separately");
        CHECK(paced->TransferMetrics().GeometryDigest == direct->TransferMetrics().GeometryDigest &&
                  paced->TransferMetrics().GeometryDigest != 0,
              "paced and direct schedules prepare identical geometry");
        CHECK(direct->Draw(error) && paced->Draw(error), "both schedules render");
        directRenderer.WaitForGpu();
        pacedRenderer.WaitForGpu();
        std::vector<float> directPixels, pacedPixels;
        CHECK(directRenderer.ReadSceneLinear(directPixels) == Render::ReadState::Ready &&
                  pacedRenderer.ReadSceneLinear(pacedPixels) == Render::ReadState::Ready &&
                  !directPixels.empty() && directPixels == pacedPixels,
              "both schedules produce identical scene-linear pixels");
      }
    }
  }
  SDL_Quit();
  return Report();
}
