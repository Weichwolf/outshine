#include "Check.h"
#include "SceneRenderer.h"
#include <SDL3/SDL.h>
#include <algorithm>
#include <array>
#include <cstdint>
#include <span>
#include <print>
#include <string>
#include <vector>

namespace {
using namespace outshine;
using namespace outshine::Render;
using namespace outshine::Test;

bool Selected(SceneRenderer &renderer, uint32_t count) {
  const auto frame = renderer.RenderFrame();
  CHECK(frame, frame ? "the current-camera frame submits" : frame.error().c_str());
  if (!frame) { return false; }
  KeptDraws kept;
  CHECK(renderer.ReadGeometrySelection(kept, GeometryReadback::Counters) == ReadState::Ready,
        "current native visibility reaches the diagnostic readback");
  if (kept.ClusterWork.Accepted != count) {
    std::println(stderr, "selected={} expected={}", kept.ClusterWork.Accepted, count);
  }
  CHECK(kept.ClusterWork.Accepted == count,
        "only certified current opaque coverage can remove a hidden cluster");
  return kept.ClusterWork.Accepted == count;
}

void Exercise(bool masked, bool hasLodParent) {
  SceneRenderer renderer({.Create = [](void *, Native::DeviceOptions options) {
    options.Validation = true;
    return Native::CreateDevice(options);
  }});
  const auto plan = ExecutionPlan::Compile(
      {.Outputs = {Resource::SceneLinear, Resource::SceneDepth, Resource::FrameTex},
       .Content = {Stage::OpaqueGeometry, Stage::TemporalResolve},
       .Exposure = {},
       .Display = {},
       .Precision = {}});
  CHECK(plan, plan ? "temporal geometry includes current occlusion depth" : plan.error().c_str());
  if (!plan) { return; }
  const auto initialized = renderer.Init({.WidthPx = 128, .HeightPx = 128}, *plan);
  CHECK(initialized, initialized ? "native targets initialize" : initialized.error().c_str());
  if (!initialized) { return; }
  std::array<MaterialBinding, 2> materials;
  for (auto &material : materials) { material.Row.DoubleSided = true; }
  materials[0].Row.Alpha = masked ? AlphaMode::Masked : AlphaMode::Opaque;
  materials[0].Row.BaseColour[3] = masked ? 0.0f : 1.0f;
  std::string error;
  CHECK(renderer.SetGeometryMesh({}, error) && renderer.SetGeometryMaterials(materials, error),
        "the test owns distinct occluder and target materials");
  constexpr std::array<uint32_t, 2> slots{0, 1};
  renderer.SetNativePieceSurfaces(slots);
  const std::array<StoredVertex, 4> vertices{
      StoredVertex::Of({{-1.2f, -1.2f, -2}}, {{0, 0}}, {{0, 0, 1}}),
      StoredVertex::Of({{1.2f, -1.2f, -2}}, {{1, 0}}, {{0, 0, 1}}),
      StoredVertex::Of({{1.2f, 1.2f, -2}}, {{1, 1}}, {{0, 0, 1}}),
      StoredVertex::Of({{-1.2f, 1.2f, -2}}, {{0, 1}}, {{0, 0, 1}})};
  constexpr std::array<uint32_t, 6> indices{0, 1, 2, 0, 2, 3};
  std::array<DagCluster, 1> clusters{DagCluster{.SelfCenter = {{0, 0, -2}},
                                                .SelfRadius = 1.7f,
                                                .ParentCenter = {{0, 0, -2}},
                                                .ParentRadius = 1.7f,
                                                .ParentErr = hasLodParent ? 1.0e10f : kDagRootErr,
                                                .Count = 6}};
  PieceMesh piece;
  piece.Verts = vertices;
  piece.Indices = indices;
  piece.Clusters = clusters;
  const auto front = renderer.PlacePiece(piece, error);
  piece.Surface = PieceSurface(1);
  piece.Row[0] = piece.Row[5] = piece.Row[10] = 0.04;
  piece.Row[14] = -6;
  const auto back = renderer.PlacePiece(piece, error);
  CHECK(front != kNoPiece && back != kNoPiece, "both independent geometric sources are resident");
  if (front == kNoPiece || back == kNoPiece) { return; }
  Viewpoint eye;
  eye.YfovRad = 1;
  eye.ZNearM = 0.1;
  eye.ZFarM = 100;
  const auto lens = Lens::From(eye, 128, 128);
  CHECK(lens.has_value(), "the temporal camera has a physical lens");
  if (!lens) { return; }
  renderer.SetCamera(eye, *lens);
  if (!Selected(renderer, 2)) { return; }
  const uint32_t retained = masked || hasLodParent ? 2u : 1u;
  for (uint32_t frame = 0; frame < 4; ++frame) {
    if (!Selected(renderer, retained)) { return; }
  }
  eye.Forward = {{0, 0, 1}};
  eye.Right = {{-1, 0, 0}};
  renderer.SetCamera(eye, *lens);
  if (!Selected(renderer, 0)) { return; }
  eye.Forward = {{0, 0, -1}};
  eye.Right = {{1, 0, 0}};
  renderer.SetCamera(eye, *lens);
  if (!Selected(renderer, 2) || !Selected(renderer, retained)) { return; }
  Mat4 moved;
  moved[12] = 100;
  CHECK(renderer.SetPieceInstances(front, std::span(&moved, 1), error),
        "an occluder can move without rebuilding its geometry");
  if (!Selected(renderer, 1) || !Selected(renderer, 1)) { return; }
  std::vector<float> depth;
  CHECK(renderer.ReadDepth(depth) == ReadState::Ready &&
            std::ranges::any_of(depth, [](float value) { return value > 0; }),
        "newly revealed target geometry remains in the final depth buffer");
}
}

int main() {
  CHECK(SDL_Init(SDL_INIT_VIDEO), "native video initializes");
  Exercise(false, false);
  Exercise(true, false);
  Exercise(false, true);
  SDL_Quit();
  return outshine::Test::Report();
}
