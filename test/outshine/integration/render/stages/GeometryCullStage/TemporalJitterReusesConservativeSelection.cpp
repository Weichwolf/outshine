#include "Check.h"
#include "RuntimeScene.h"
#include "SceneRenderer.h"
#include "GeometryCullStage.h"
#include "ClusterDag.h"
#include "StoredVertex.h"
#include "GpuSubmission.h"
#include "Viewing.h"
#include <Geometry.h>
#include <Material.h>
#include <SDL3/SDL.h>
#include <algorithm>
#include <array>
#include <cstdint>
#include <cstddef>
#include <expected>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace {
using namespace outshine;
using namespace outshine::Render;
using namespace outshine::Test;

enum class DepthRequest { None, Stage, Output, Both };

void PlanTransitions(SceneRenderer &renderer, Core::RuntimeScene &scene, std::string &error) {
  const auto original = renderer.Plan().Specification();
  for (const bool temporal : {false, true}) {
    auto specification = original;
    if (!temporal) { std::erase(specification.Content, Stage::TemporalResolve); }
    auto plan = Compiled::Compile(specification);
    CHECK(plan.has_value(), "the changed temporal plan compiles");
    if (!plan) { return; }
    CHECK(renderer.Init({64, 64}, *plan).has_value(), "the same renderer accepts the changed plan");
    for (unsigned frame = 0; frame < 4; ++frame) {
      CHECK(scene.Draw(error), "the resident scene draws after the temporal plan changes");
      CHECK(renderer.Plan().Holds(Stage::TemporalResolve) == temporal,
            "drawing retains the selected temporal mode");
      std::vector<float> depth;
      CHECK(renderer.ReadDepth(depth) == ReadState::Ready &&
                std::ranges::any_of(depth, [](float value) { return value > 0; }),
            "changing temporal mode preserves the visible geometry");
    }
  }
}

void Exercise(DepthRequest requestedDepth) {
  constexpr double kNearM = 0.1;
  constexpr double kFarM = 100;
  constexpr double kMovedEyeXM = 0.1;
  constexpr double kNarrowFovRad = 0.9;
  Geometry base;
  Material material;
  material.Unlit = true;
  const auto surface = base.addSurface("opaque", material);
  CHECK(surface.has_value(), "the clustered piece has a real material");
  if (!surface) { return; }
  const auto part = base.addPart("background", *surface);
  CHECK(part && base.setPositions(*part, std::array<float, 9>{-1, -1, -8, 1, -1, -8, 0, 1, -8}) &&
            base.setTriangles(*part, std::array<uint32_t, 3>{0, 1, 2}),
        "the scene supplies a valid subject pipeline");
  Core::Declaration declaration;
  declaration.InitialGeometry = &base;
  declaration.SurfaceWidthPx = declaration.SurfaceHeightPx = 64;
  declaration.Stages = {"subjects", "temporalResolve"};
  declaration.Outputs = {"surface", "sceneDepth", "sceneVelocity"};
  if (requestedDepth == DepthRequest::Stage || requestedDepth == DepthRequest::Both) {
    declaration.Stages.emplace_back("depthPyramid");
  }
  if (requestedDepth == DepthRequest::Output || requestedDepth == DepthRequest::Both) {
    declaration.Outputs.emplace_back("depthPyramid");
  }
  bool reject = false;
  SceneRenderer renderer(
      {.Context = &reject,
       .Submit =
           [](void *context, Native::Device *device, std::unique_ptr<Native::Commands> commands)
           -> Native::Result<Native::Completion> {
         if (std::exchange(*static_cast<bool *>(context), false)) {
           return std::unexpected(
               Native::Error{.Kind = Native::ErrorKind::Driver, .Message = "rejected frame"});
         }
         return device->Submit(std::move(commands));
       }});
  std::unique_ptr<Core::RuntimeScene> scene;
  std::string error;
  CHECK(Core::RuntimeScene::Open(renderer, declaration, nullptr, scene, error), "TAA scene opens");
  if (!scene) { return; }
  CHECK(renderer.Plan().Holds(Stage::TemporalResolve), "the real temporal stage is enabled");
  constexpr std::array<Vec3f, 3> points{
      {{{-0.3f, -0.3f, -4}}, {{0.3f, -0.3f, -4}}, {{0, 0.3f, -4}}}};
  std::array<StoredVertex, 3> vertices{};
  for (size_t at = 0; at < vertices.size(); ++at) {
    vertices[at] = StoredVertex::Of(points[at], {{0, 0}}, {{0, 0, 1}});
  }
  constexpr std::array<uint32_t, 3> indices{0, 1, 2};
  const std::array<DagCluster, 1> clusters{{{.SelfCenter = {{0, 0, -4}},
                                             .SelfRadius = 0.5f,
                                             .ParentCenter = {{0, 0, -4}},
                                             .ParentRadius = 0.5f,
                                             .ParentErr = kDagRootErr,
                                             .Count = indices.size()}}};
  PieceMesh piece{};
  piece.Verts = vertices;
  piece.Indices = indices;
  piece.Clusters = clusters;
  CHECK(renderer.PlacePiece(piece, error) != kNoPiece, "a real clustered triangle exists");
  Viewpoint eye;
  eye.ZNearM = kNearM;
  eye.ZFarM = kFarM;
  eye.YfovRad = 1;
  scene->Eye(eye);
  CHECK(scene->Draw(error) && GeometryCullStage::JobsSweptTaken() > 0,
        "the initial selection is actually generated");
  std::vector<float> initial;
  CHECK(renderer.ReadDepth(initial) == ReadState::Ready &&
            std::ranges::any_of(initial, [](float value) { return value > 0; }),
        "the clustered triangle produces real depth");
  bool rasterChanged = false;
  for (unsigned at = 0; at < 32; ++at) {
    CHECK(scene->Draw(error) && GeometryCullStage::JobsSweptTaken() == 0,
          "two complete jitter periods reuse selection without GPU cull/scan/compact");
    std::vector<float> depth;
    CHECK(renderer.ReadDepth(depth) == ReadState::Ready, "jittered depth is readable");
    rasterChanged |= depth != initial;
  }
  CHECK(rasterChanged, "raster jitter continues while selection is reused");
  eye.EyeM[0] = kMovedEyeXM;
  scene->Eye(eye);
  reject = true;
  CHECK(!scene->Draw(error) && !reject, "a moved frame can be rejected after selection recording");
  CHECK(scene->Draw(error) && GeometryCullStage::JobsSweptTaken() > 0,
        "retry never reuses an unsubmitted selection");
  CHECK(scene->Draw(error) && GeometryCullStage::JobsSweptTaken() == 0,
        "the accepted moved view settles again");
  eye.YfovRad = kNarrowFovRad;
  scene->Eye(eye);
  CHECK(scene->Draw(error) && GeometryCullStage::JobsSweptTaken() > 0,
        "projection changes invalidate the selection");
  PlanTransitions(renderer, *scene, error);
}
}

int main() {
  CHECK(SDL_Init(SDL_INIT_VIDEO), "video initializes");
  for (const auto depth :
       {DepthRequest::None, DepthRequest::Stage, DepthRequest::Output, DepthRequest::Both}) {
    Exercise(depth);
  }
  SDL_Quit();
  return outshine::Test::Report();
}
