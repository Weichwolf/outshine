#include "Check.h"
#include "RuntimeScene.h"
#include "SceneRenderer.h"
#include "ClusterDag.h"
#include "StoredVertex.h"
#include "PieceMesh.h"
#include "Viewing.h"
#include <Geometry.h>
#include <Material.h>
#include <SDL3/SDL.h>
#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <memory>
#include <print>
#include <string>
#include <vector>

namespace {
using namespace outshine;
using namespace outshine::Render;
using namespace outshine::Test;
constexpr size_t kExtent = 128;
constexpr float kFarM = 100;
constexpr double kNearM = 0.1;
constexpr double kTranslationM = 2.1;
constexpr double kShearedY = 0.25;
constexpr std::array<Vec3f, 3> kPoints{
    {{{-0.7f, -0.7f, -2}}, {{-0.7f, -0.5f, -2}}, {{-0.5f, -0.7f, -2}}}};

struct PlacementCase {
  float LocalScale = 1;
  float HomogeneousW = 1;
};

bool Draw(bool clustered,
          PlacementCase example,
          std::vector<float> &image,
          std::vector<float> &depth) {
  const float localScale = example.LocalScale;
  const float homogeneousW = example.HomogeneousW;
  Geometry geometry;
  Material material;
  material.Unlit = true;
  material.DoubleSided = true;
  const auto surface = geometry.addSurface("white", material);
  if (!surface) { return false; }
  const auto part = geometry.addPart("offscreen-control", *surface);
  constexpr std::array<float, 9> offscreen{-10, -10, -2, -9, -10, -2, -10, -9, -2};
  constexpr std::array<uint32_t, 3> indices{0, 1, 2};
  if (!part || !geometry.setPositions(*part, offscreen) || !geometry.setTriangles(*part, indices)) {
    return false;
  }
  Core::Declaration declaration;
  declaration.InitialGeometry = &geometry;
  declaration.SurfaceWidthPx = declaration.SurfaceHeightPx = kExtent;
  declaration.Outputs = {"sceneLinear", "depthPyramid"};
  declaration.Stages = {"opaqueGeometry", "depthPyramid"};
  SceneRenderer renderer;
  std::unique_ptr<Core::RuntimeScene> scene;
  std::string error;
  if (!Core::RuntimeScene::Open(renderer, declaration, nullptr, scene, error)) {
    std::println(stderr, "open: {}", error);
    return false;
  }
  Viewpoint eye;
  eye.Kind = CameraKind::Orthographic;
  eye.XMagM = eye.YMagM = 1;
  eye.ZNearM = kNearM;
  eye.ZFarM = kFarM;
  scene->Eye(eye);
  std::array<StoredVertex, 3> vertices{};
  for (size_t at = 0; at < vertices.size(); ++at) {
    auto point = kPoints[at];
    for (size_t axis = 0; axis < 3; ++axis) { point[axis] *= localScale; }
    vertices[at] = StoredVertex::Of(point, {{0, 0}}, {{0, 0, 1}});
  }
  const std::array<DagCluster, 1> clusters{{{.SelfCenter = {{0, 0, -2 * localScale}},
                                             .SelfRadius = localScale,
                                             .ParentCenter = {{0, 0, -2 * localScale}},
                                             .ParentRadius = localScale,
                                             .ParentErr = kDagRootErr,
                                             .Count = indices.size()}}};
  PieceMesh piece;
  piece.Verts = vertices;
  piece.Indices = indices;
  if (clustered) { piece.Clusters = clusters; }
  piece.Row.At(0, 0) = piece.Row.At(0, 1) = piece.Row.At(2, 2) = 1.0 / localScale;
  piece.Row.At(1, 1) = kShearedY / localScale;
  piece.Row.At(0, 3) = kTranslationM;
  piece.Row.At(3, 3) = homogeneousW;
  if (renderer.PlacePiece(piece, error) == kNoPiece || !scene->Draw(error)) {
    std::println(stderr, "draw: {}", error);
    return false;
  }
  renderer.WaitForGpu();
  return renderer.ReadSceneLinear(image) == ReadState::Ready &&
         renderer.ReadDepth(depth) == ReadState::Ready;
}
}

int main() {
  CHECK(SDL_Init(SDL_INIT_VIDEO), "video initializes for the affine culling counterexample");
  std::vector<float> control;
  std::vector<float> controlDepth;
  std::vector<float> clustered;
  std::vector<float> clusteredDepth;
  constexpr std::array<PlacementCase, 4> placements{
      {{}, {.LocalScale = 1e20f}, {.LocalScale = 1e-20f}, {.HomogeneousW = 2}}};
  for (const auto &placement : placements) {
    CHECK(Draw(false, placement, control, controlDepth), "unclustered geometry draws natively");
    CHECK(std::ranges::any_of(controlDepth, [](float value) { return value > 0; }),
          "the independently drawn triangle has visible pixels inside the frame");
    CHECK(Draw(true, placement, clustered, clusteredDepth),
          "the same geometry and placement draw with a cluster");
    CHECK(clustered == control && clusteredDepth == controlDepth,
          "a local sphere cannot discard visible geometry under shear or unknown projection");
  }
  SDL_Quit();
  return Report();
}
