#include "Check.h"
#include "SceneRenderer.h"
#include "GeometryCullStage.h"
#include <SDL3/SDL.h>
#include <algorithm>
#include <array>
#include <cstdint>
#include <cstddef>
#include <span>
#include <string>
#include <vector>

namespace {
using namespace outshine;
using namespace outshine::Render;
using namespace outshine::Test;

bool Draw(SceneRenderer &renderer) {
  const auto frame = renderer.RenderFrame();
  CHECK(frame, frame ? "the complete native frame graph submits" : frame.error().c_str());
  return frame.has_value();
}

void Exercise() {
  SceneRenderer renderer;
  const auto plan =
      ExecutionPlan::Compile({.Outputs = {Resource::SceneLinear, Resource::SceneDepth},
                              .Content = {Stage::OpaqueGeometry},
                              .Exposure = {},
                              .Display = {},
                              .Precision = {}});
  CHECK(plan && (*plan)->Holds(Stage::GeometryRootCull) && (*plan)->Holds(Stage::GeometryCull),
        "the actual opaque plan owns the root and indirect leaf passes");
  if (!plan) { return; }
  const auto initialized = renderer.Init({.WidthPx = 64, .HeightPx = 64}, *plan);
  CHECK(initialized, initialized ? "the native renderer initializes" : initialized.error().c_str());
  if (!initialized) { return; }
  MaterialBinding material;
  material.Row.Unlit = true;
  material.Row.DoubleSided = true;
  material.Row.BaseColour = {{0.25f, 0.5f, 0.75f, 1}};
  std::string error;
  CHECK(renderer.SetGeometryMesh({}, error) &&
            renderer.SetGeometryMaterials(std::span(&material, 1), error),
        "the scene owns its source geometry and material");
  constexpr std::array<uint32_t, 1> slots{0};
  renderer.SetNativePieceSurfaces(slots);
  const std::array<StoredVertex, 3> vertices{
      StoredVertex::Of({{-0.5f, -0.5f, -2}}, {{0, 0}}, {{0, 0, 1}}),
      StoredVertex::Of({{0.5f, -0.5f, -2}}, {{1, 0}}, {{0, 0, 1}}),
      StoredVertex::Of({{0, 0.5f, -2}}, {{0, 1}}, {{0, 0, 1}})};
  constexpr std::array<uint32_t, 3> indices{0, 1, 2};
  const std::array<DagCluster, 1> clusters{DagCluster{.SelfCenter = {{0, 0, -2}},
                                                      .SelfRadius = 1,
                                                      .ParentCenter = {{0, 0, -2}},
                                                      .ParentRadius = 1,
                                                      .ParentErr = kDagRootErr,
                                                      .Count = 3}};
  std::array<Mat4, 2> rows{};
  rows[0][12] = 100;
  PieceMesh piece;
  piece.Verts = vertices;
  piece.Indices = indices;
  piece.Clusters = clusters;
  piece.Instances = rows;
  const auto id = renderer.PlacePiece(piece, error);
  CHECK(id != kNoPiece, "the clustered source product is resident");
  if (id == kNoPiece) { return; }
  Viewpoint eye;
  eye.YfovRad = 1;
  eye.ZNearM = 0.1;
  eye.ZFarM = 100;
  const auto lens = Lens::From(eye, 64, 64);
  CHECK(lens.has_value(), "the camera has a valid physical projection");
  if (!lens) { return; }
  renderer.SetCamera(eye, *lens);
  if (!Draw(renderer)) { return; }
  KeptDraws visible;
  std::vector<float> first;
  CHECK(renderer.ReadKeptIndices(visible) == ReadState::Ready && visible.Indices == 3 &&
            visible.Visibility == std::vector<uint32_t>({0, 3}) &&
            visible.DrawIndex == std::vector<uint32_t>({0, 1, 2}),
        "the actual frame graph selects and copies the original triangle");
  std::vector<float> depth;
  CHECK(renderer.ReadSceneLinear(first) == ReadState::Ready &&
            renderer.ReadDepth(depth) == ReadState::Ready &&
            std::ranges::any_of(depth, [](float value) { return value > 0; }),
        "the submitted triangle produces actual native depth coverage");
  size_t coloured = 0;
  for (size_t at = 0; at + 3 < first.size(); at += 4) {
    coloured += first[at] > 0 || first[at + 1] > 0 || first[at + 2] > 0 ? 1u : 0u;
  }
  CHECK(coloured > 0, "visible geometry produces colour independently of background alpha");
  rows[1][12] = 100;
  CHECK(renderer.SetPieceInstances(id, rows, error), "instance movement updates the resident view");
  if (!Draw(renderer)) { return; }
  KeptDraws hidden;
  CHECK(renderer.ReadKeptIndices(hidden) == ReadState::Ready && hidden.Indices == 0 &&
            hidden.Visibility == std::vector<uint32_t>({0, 0}),
        "a rejected root cannot expose the preceding frame's stale Kept values");
  rows[1][12] = 0;
  CHECK(renderer.SetPieceInstances(id, rows, error),
        "the same instance returns without rebuilding its product");
  if (!Draw(renderer)) { return; }
  std::vector<float> returned;
  CHECK(renderer.ReadSceneLinear(returned) == ReadState::Ready && returned == first,
        "returning visibility restores the exact original native frame");
}
}

int main() {
  CHECK(SDL_Init(SDL_INIT_VIDEO), "native video support initializes");
  Exercise();
  SDL_Quit();
  return outshine::Test::Report();
}
