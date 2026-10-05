#include "Check.h"
#include "RuntimeScene.h"
#include "SceneRenderer.h"
#include "SubjectCullStage.h"
#include "ClusterDag.h"
#include "StoredVertex.h"
#include "SubjectTypes.h"
#include "GpuSubmission.h"
#include "Viewing.h"
#include <scene/Geometry.h>
#include <scene/Material.h>
#include <SDL3/SDL.h>
#include <algorithm>
#include <array>
#include <cmath>
#include <memory>
#include <string>
#include <vector>
#include <utility>

namespace {
bool AddTarget(outshine::Render::SceneRenderer &renderer, std::string &error) {
  using namespace outshine;
  using namespace outshine::Render;
  const std::array<Vec3f, 4> points{
      {{{-0.2f, -0.2f, -12}}, {{0.2f, -0.2f, -12}}, {{0.2f, 0.2f, -12}}, {{-0.2f, 0.2f, -12}}}};
  std::array<StoredVertex, 4> vertices{};
  for (size_t at = 0; at < vertices.size(); ++at) {
    vertices[at] = StoredVertex::Of(points[at], {{0, 0}}, {{0, 0, 1}});
  }
  constexpr std::array<uint32_t, 6> indices{0, 1, 2, 0, 2, 3};
  const std::array<DagCluster, 1> clusters{{{.SelfCenter = {{0, 0, -12}},
                                             .SelfRadius = 0.3f,
                                             .ParentCenter = {{0, 0, -12}},
                                             .ParentRadius = 0.3f,
                                             .ParentErr = kDagRootErr,
                                             .Count = indices.size()}}};
  return renderer.PlacePiece({.Verts = vertices, .Indices = indices, .Clusters = clusters},
                             error) != kNoPiece;
}

void RefineStaticView(outshine::Core::RuntimeScene &scene,
                      outshine::Render::SceneRenderer &renderer,
                      std::string &error) {
  using namespace outshine::Render;
  using namespace outshine::Test;
  KeptDraws initial;
  KeptDraws refined;
  std::vector<float> before;
  std::vector<float> after;
  CHECK(renderer.ReadKeptIndices(initial) == ReadState::Ready &&
            renderer.ReadSceneLinear(before) == ReadState::Ready,
        "first-frame draw input and pixels are readable");
  CHECK(scene.Draw(error), "an unchanged view refines against its genuine depth history");
  CHECK(renderer.ReadKeptIndices(refined) == ReadState::Ready &&
            renderer.ReadSceneLinear(after) == ReadState::Ready,
        "refined draw input and pixels are readable");
  CHECK(refined.Indices < initial.Indices,
        "stationary HiZ removes the hidden target instead of disabling occlusion");
  CHECK(before == after, "removing hidden geometry preserves the image exactly");
  CHECK(scene.Draw(error) && SubjectCullStage::JobsSweptTaken() == 0,
        "a settled unchanged view reuses its refined draw input");
}

void ReplaceMaterials(outshine::Core::RuntimeScene &scene,
                      outshine::Render::SceneRenderer &renderer,
                      std::string &error) {
  using namespace outshine;
  using namespace outshine::Render;
  using namespace outshine::Test;
  CHECK(scene.Draw(error), "unchanged selection settles before a table replacement");
  SubjectMaterial red;
  red.Row.Unlit = true;
  red.Row.BaseColour = {{1, 0, 0, 1}};
  const std::array<SubjectMaterial, 1> materials{red};
  CHECK(renderer.SetSubjectMaterials(materials, error) && scene.Draw(error),
        "replacement native materials remain drawable");
  CHECK(SubjectCullStage::JobsSweptTaken() > 0,
        "rebuilt draw tables invalidate cached GPU selection");
  std::vector<float> depth;
  CHECK(renderer.ReadDepth(depth) == ReadState::Ready &&
            std::ranges::any_of(depth, [](float value) { return value > 0; }),
        "an unchanged visible target survives material table replacement");
}
}

int main() {
  using namespace outshine;
  using namespace outshine::Render;
  using namespace outshine::Test;
  CHECK(SDL_Init(SDL_INIT_VIDEO), "video initializes for disocclusion");
  {
    Geometry geometry;
    Material material;
    material.Unlit = true;
    const auto surface = geometry.addSurface("opaque-wall", material);
    CHECK(surface.has_value(), "opaque material exists");
    if (!surface) { return Report(); }
    const auto part = geometry.addPart("near-occluder", *surface);
    CHECK(part.has_value(), "occluder exists");
    if (!part) { return Report(); }
    CHECK(geometry.setPositions(
              *part, std::array<float, 12>{-1, -1, -2, 1, -1, -2, 1, 1, -2, -1, 1, -2}) &&
              geometry.setTriangles(*part, std::array<uint32_t, 6>{0, 1, 2, 0, 2, 3}),
          "near wall covers the old screen centre");
    Core::Declaration declaration;
    declaration.InitialGeometry = &geometry;
    declaration.SurfaceWidthPx = declaration.SurfaceHeightPx = 128;
    declaration.Outputs = {"sceneLinear", "depthPyramid"};
    declaration.Stages = {"subjects", "depthPyramid"};
    bool rejectSubmission = false;
    SceneRenderer history(
        {.Context = &rejectSubmission,
         .Submit = [](void *context, SDL_GPUCommandBuffer *commands) -> SDL_GPUFence * {
           if (std::exchange(*static_cast<bool *>(context), false)) {
             SDL_CancelGPUCommandBuffer(commands);
             SDL_SetError("injected culling submission failure");
             return nullptr;
           }
           return SDL_SubmitGPUCommandBufferAndAcquireFence(commands);
         }});
    SceneRenderer control;
    std::unique_ptr<Core::RuntimeScene> historyScene;
    std::unique_ptr<Core::RuntimeScene> controlScene;
    std::string error;
    CHECK(Core::RuntimeScene::Open(history, declaration, nullptr, historyScene, error) &&
              Core::RuntimeScene::Open(control, declaration, nullptr, controlScene, error),
          "history and first-frame oracle have identical geometry");
    if (!historyScene || !controlScene) { return Report(); }
    CHECK(AddTarget(history, error) && AddTarget(control, error),
          "independent clustered target is behind the wall");
    Viewpoint eye;
    eye.YfovRad = 1;
    eye.ZNearM = 0.1;
    eye.ZFarM = 100;
    historyScene->Eye(eye);
    CHECK(historyScene->Draw(error), "old view produces genuine depth history");
    history.WaitForGpu();
    std::vector<float> oldDepth;
    CHECK(history.ReadDepth(oldDepth) == ReadState::Ready && oldDepth.size() == 128u * 128u &&
              oldDepth[64u * 128u + 64u] > 0.04f,
          "the old wall is a nonempty occluder, not a vacuous control");
    RefineStaticView(*historyScene, history, error);
    eye.EyeM[0] = 3;
    historyScene->Eye(eye);
    controlScene->Eye(eye);
    CHECK(historyScene->Draw(error) && controlScene->Draw(error),
          "camera translates beyond the wall and exposes the distant target");
    std::vector<float> actual;
    std::vector<float> expected;
    CHECK(history.ReadSceneLinear(actual) == ReadState::Ready &&
              control.ReadSceneLinear(expected) == ReadState::Ready,
          "both new views are readable");
    std::vector<float> revealedDepth;
    const double expectedDepth = eye.ZNearM * (eye.ZFarM / 12.0 - 1.0) / (eye.ZFarM - eye.ZNearM);
    CHECK(control.ReadDepth(revealedDepth) == ReadState::Ready &&
              std::ranges::any_of(revealedDepth,
                                  [expectedDepth](float value) {
                                    return std::abs(value - expectedDepth) < 1.0e-6;
                                  }),
          "the independent new view contains the target at twelve metres");
    CHECK(actual == expected, "previous-view depth cannot erase geometry visible in the new view");
    eye.EyeM[0] = 3.5;
    historyScene->Eye(eye);
    controlScene->Eye(eye);
    rejectSubmission = true;
    CHECK(!historyScene->Draw(error), "failed submission does not publish a culling result");
    CHECK(historyScene->Draw(error) && controlScene->Draw(error),
          "the failed moved view is retryable");
    CHECK(history.ReadSceneLinear(actual) == ReadState::Ready &&
              control.ReadSceneLinear(expected) == ReadState::Ready && actual == expected,
          "retry recomputes the moved view instead of reusing unsubmitted selection");
    ReplaceMaterials(*historyScene, history, error);
  }
  SDL_Quit();
  return Report();
}
