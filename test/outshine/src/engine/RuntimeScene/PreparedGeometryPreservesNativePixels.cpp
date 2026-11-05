#include "RuntimeScene.h"
#include "SceneRenderer.h"
#include "ClusterCook.h"
#include "Check.h"
#include <SDL3/SDL.h>
#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace {
using namespace outshine;
using namespace outshine::Test;

Geometry Surface() {
  Geometry result;
  CHECK(result.addSurface("ground", Material{}), "the world has a separate ground material");
  Material material;
  material.Unlit = true;
  const auto surface = result.addSurface("surface", material);
  CHECK(surface, "a prepared product has its own material");
  if (!surface) { return result; }
  const auto part = result.addPart("surface", *surface);
  CHECK(part, "a prepared product has one mesh part");
  if (!part) { return result; }
  CHECK(result.setPositions(*part, std::array<float, 12>{-1, -1, 0, 1, -1, 0, 1, 1, 0, -1, 1, 0}) &&
            result.setNormals(*part, std::array<float, 12>{0, 0, 1, 0, 0, 1, 0, 0, 1, 0, 0, 1}) &&
            result.setTriangles(*part, std::array<uint32_t, 6>{0, 1, 2, 0, 2, 3}),
        "the source has indexed geometry and complete source-frame attributes");
  return result;
}

void Compare() {
  const Geometry seed = Surface();
  auto product = Surface();
  CHECK(product.setPositions(
            0, std::array<float, 12>{-2, -1, 0.25f, 2, -1, 0.25f, 2, 2, 0.25f, -2, 2, 0.25f}) &&
            product.setColours(
                0, std::array<float, 16>{1, 0, 0, 1, 1, 0, 0, 1, 1, 0, 0, 1, 1, 0, 0, 1}),
        "the generated product differs visibly from the driven seed");
  auto prepared = CookClusters(
      {.PositionsM = product.positionsOf(0), .Indices = product.trianglesOf(0)}, kClusterTriangles);
  CHECK(prepared && product.setTriangles(0, prepared->Index),
        "the producer finishes its index order");
  if (!prepared) { return; }
  GeometryClusters clusters;
  clusters.push_back(std::move(prepared->Clusters));
  Core::Declaration declaration;
  declaration.InitialGeometry = &seed;
  declaration.SurfaceWidthPx = declaration.SurfaceHeightPx = 32;
  declaration.Outputs = {"sceneLinear"};
  Render::SceneRenderer referenceRenderer;
  Render::SceneRenderer restoredRenderer;
  std::unique_ptr<Core::RuntimeScene> reference;
  std::unique_ptr<Core::RuntimeScene> restored;
  std::string error;
  CHECK(Core::RuntimeScene::Open(referenceRenderer, declaration, nullptr, reference, error),
        "the reference scene opens");
  CHECK(Core::RuntimeScene::Open(restoredRenderer, declaration, nullptr, restored, error),
        "the prepared scene opens");
  if (!reference || !restored) { return; }
  CHECK(reference->Draw(error) && restored->Draw(error), "both initial worlds render");
  referenceRenderer.WaitForGpu();
  restoredRenderer.WaitForGpu();
  std::vector<float> seedPixels;
  CHECK(referenceRenderer.ReadSceneLinear(seedPixels) == Render::ReadState::Ready,
        "the initial world has a native pixel control");
  const Material material;
  CHECK(reference->BeginGeneratedGeometryBuild(product.clone(), MaterialInstance(0), material),
        "the ordinary product starts native preparation");
  CHECK(restored->BeginGeneratedGeometryBuild(
            std::move(product), MaterialInstance(0), material, std::move(clusters)),
        "the cached product starts without rebuilding its clusters");
  for (auto *scene : {reference.get(), restored.get()}) {
    for (size_t call = 0; scene->GeometryBuildActive() && call < 10000; ++call) {
      const auto advanced = scene->AdvanceGeometryBuild(64);
      CHECK(advanced, "native geometry publication advances");
      if (!advanced) { return; }
    }
    CHECK(!scene->GeometryBuildActive(), "native geometry publication completes");
  }
  CHECK(reference->Clustering().CookedParts == 2 && restored->Clustering().CookedParts == 1 &&
            restored->Clustering().RestoredParts == 1,
        "the driven prefix is prepared while the supplied product bypasses cooking");
  const bool referenceDrawn = reference->Draw(error);
  CHECK(referenceDrawn, referenceDrawn ? "the reference scene renders" : error.c_str());
  const bool restoredDrawn = restored->Draw(error);
  CHECK(restoredDrawn, restoredDrawn ? "the restored scene renders" : error.c_str());
  referenceRenderer.WaitForGpu();
  restoredRenderer.WaitForGpu();
  std::vector<float> before;
  std::vector<float> after;
  CHECK(referenceRenderer.ReadSceneLinear(before) == Render::ReadState::Ready &&
            restoredRenderer.ReadSceneLinear(after) == Render::ReadState::Ready &&
            !before.empty() && before == after,
        "prepared clusters preserve native pixels with the driven prefix");
  CHECK(before != seedPixels, "the generated geometry contributes to the compared image");
}
}

int main() {
  const bool initialized = SDL_Init(SDL_INIT_VIDEO);
  CHECK(initialized, "video initializes");
  if (initialized) { Compare(); }
  SDL_Quit();
  return outshine::Test::Report();
}
