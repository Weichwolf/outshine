#include "Check.h"
#include "ClusterDag.h"
#include "GpuSubmission.h"
#include "RuntimeScene.h"
#include "SceneRenderer.h"
#include "StoredVertex.h"
#include "GeometryCullStage.h"
#include <Geometry.h>
#include <Material.h>
#include <SDL3/SDL.h>
#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace {
using namespace outshine;
using namespace outshine::Render;
using namespace outshine::Test;
constexpr double kNearM = 0.1;
constexpr double kFarM = 100;

void Exercise() {
  Geometry base;
  Material material;
  material.Unlit = true;
  const auto surface = base.addSurface("opaque", material);
  CHECK(surface.has_value(), "preparation has a real opaque material");
  if (!surface) { return; }
  const auto part = base.addPart("background", *surface);
  CHECK(part && base.setPositions(*part, std::array<float, 9>{-1, -1, -8, 1, -1, -8, 0, 1, -8}) &&
            base.setTriangles(*part, std::array<uint32_t, 3>{0, 1, 2}),
        "preparation has a real geometry pipeline");
  Core::Declaration declaration;
  declaration.InitialGeometry = &base;
  declaration.SurfaceWidthPx = declaration.SurfaceHeightPx = 64;
  declaration.Stages = {"opaqueGeometry", "depthPyramid"};
  declaration.Outputs = {"surface", "sceneDepth", "depthPyramid"};
  bool reject = false;
  SceneRenderer renderer(
      {.Context = &reject,
       .Submit =
           [](void *context, Native::Device *device, std::unique_ptr<Native::Commands> commands)
           -> Native::Result<Native::Completion> {
         if (std::exchange(*static_cast<bool *>(context), false)) {
           return std::unexpected(
               Native::Error{.Kind = Native::ErrorKind::Driver, .Message = "rejected preparation"});
         }
         return device->Submit(std::move(commands));
       }});
  std::unique_ptr<Core::RuntimeScene> scene;
  std::string error;
  CHECK(Core::RuntimeScene::Open(renderer, declaration, nullptr, scene, error),
        "preparation scene opens");
  if (!scene) { return; }
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
  eye.YfovRad = 1;
  eye.ZNearM = kNearM;
  eye.ZFarM = kFarM;
  scene->Eye(eye);
  CHECK(scene->Advance(error), "the scene camera binds with its world anchor before rendering");
  renderer.ResetMeasurements();
  reject = true;
  CHECK(!renderer.PrepareWorldResources() && !reject && !renderer.Drew(),
        "a rejected preparation publishes no image or selection");
  const auto prepared = renderer.PrepareWorldResources();
  CHECK(prepared && GeometryCullStage::JobsAdmittedTaken() > 0,
        "successful preparation actually runs cull, scan and compaction");
  if (!prepared) { return; }
  renderer.WaitForGpu();
  CHECK(renderer.WorldResourcesComplete(*prepared), "prepared GPU selection is complete");
  KeptDraws first;
  CHECK(renderer.ReadGeometrySelection(first, outshine::Render::GeometryReadback::Indices) ==
                ReadState::Ready &&
            first.Indices > 0,
        "prepared draw arguments and indices exist before any frame");
  const auto repeated = renderer.PrepareWorldResources();
  CHECK(repeated && GeometryCullStage::JobsAdmittedTaken() == 0,
        "preparation reuses accepted selection without inventing previous depth");
  renderer.WaitForGpu();
  CHECK(!renderer.Drew() && renderer.LastSubmittedCamera().Serial == 0 &&
            renderer.Costs().Frames == 0,
        "preparation performs no world frames or camera publication");
  CHECK(renderer.RenderFrame().has_value() && GeometryCullStage::JobsAdmittedTaken() == 0,
        "the first real frame reuses the prepared selection");
  std::vector<float> depth;
  CHECK(renderer.ReadDepth(depth) == ReadState::Ready &&
            std::ranges::any_of(depth, [](float z) { return z > 0; }),
        "only the real frame produces readable geometry depth");
  CHECK(renderer.LastSubmittedCamera().Serial == 1 && renderer.Costs().Frames == 1,
        "only one world frame was measured and published");
  CHECK(renderer.RenderFrame().has_value() && GeometryCullStage::JobsAdmittedTaken() > 0,
        "the next stable frame can refine against genuine previous depth");
}
}

int main() {
  CHECK(SDL_Init(SDL_INIT_VIDEO), "video initializes");
  Exercise();
  SDL_Quit();
  return Report();
}
