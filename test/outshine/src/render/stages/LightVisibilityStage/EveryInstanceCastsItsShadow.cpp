#include "Check.h"
#include "RuntimeScene.h"
#include "SceneRenderer.h"
#include <SDL3/SDL.h>
#include <array>
#include <memory>
#include <string>
#include <vector>

namespace {
constexpr std::array<uint32_t, 36> kBoxIndices{0, 2, 1, 0, 3, 2, 4, 5, 6, 4, 6, 7,
                                               0, 1, 5, 0, 5, 4, 3, 7, 6, 3, 6, 2,
                                               0, 4, 7, 0, 7, 3, 1, 2, 6, 1, 6, 5};

bool CastAt(const std::vector<float> &atlas, int x) {
  using namespace outshine::Render;
  if (atlas.size() != size_t(kShadowAtlasPx) * kShadowAtlasPx) { return false; }
  const int column = kShadowTilePx / 2 - x * kShadowTilePx / 512;
  return atlas[size_t(kShadowTilePx / 2) * kShadowAtlasPx + size_t(column)] > 0;
}

void CheckInstances(bool clustered) {
  using namespace outshine;
  using namespace outshine::Render;
  using namespace outshine::Test;
  constexpr std::array<float, 24> positions{-8, -8, -8, 8, -8, -8, 8, 8, -8, -8, 8, -8,
                                            -8, -8, 8,  8, -8, 8,  8, 8, 8,  -8, 8, 8};
  Geometry geometry;
  const auto surface = geometry.addSurface("caster", Material{});
  CHECK(surface.has_value(), "native caster material exists");
  if (!surface) { return; }
  const auto part = geometry.addPart("distant-material-owner", *surface);
  CHECK(part.has_value(), "native material owner exists");
  if (!part) { return; }
  Mat4 far;
  far[12] = 10000;
  CHECK(geometry.setPositions(*part, positions) && geometry.setTriangles(*part, kBoxIndices) &&
            geometry.setPlacement(*part, far).has_value(),
        "material owner lies outside the near shadow tile");
  Core::Declaration declaration;
  declaration.InitialGeometry = &geometry;
  declaration.SurfaceWidthPx = declaration.SurfaceHeightPx = 32;
  declaration.Outputs = {"surface", "sceneLinear", "shadowAtlas"};
  declaration.DrawsSky = true;
  declaration.ShadowRadiusM = 16384;
  declaration.KeyLux = 10000;
  SceneRenderer renderer;
  std::unique_ptr<Core::RuntimeScene> scene;
  std::string error;
  CHECK(Core::RuntimeScene::Open(renderer, declaration, nullptr, scene, error),
        "instance shadow world opens");
  if (!scene) { return; }
  Viewpoint eye;
  eye.EyeM = {{0, 0, 100}};
  eye.YfovRad = 1;
  eye.ZNearM = 0.1;
  eye.ZFarM = 20000;
  scene->Eye(eye);
  renderer.SetShadowFrame({{0, 0, 1}}, {{0, 1, 0}}, 16384, true);
  std::array<StoredVertex, 8> vertices{};
  for (size_t at = 0; at < vertices.size(); ++at) {
    vertices[at] = StoredVertex::Of(
        {{positions[at * 3], positions[at * 3 + 1], positions[at * 3 + 2]}}, {{0, 0}}, {{0, 0, 1}});
  }
  std::array<Mat4, 2> rows{};
  rows[0][12] = -80;
  rows[1][12] = 80;
  const std::array<DagCluster, 1> clusters{{{.SelfRadius = 14,
                                             .ParentRadius = 14,
                                             .ParentErr = kDagRootErr,
                                             .Count = kBoxIndices.size()}}};
  PieceMesh piece{.Verts = vertices, .Indices = kBoxIndices, .Instances = rows};
  if (clustered) { piece.Clusters = clusters; }
  const auto id = renderer.PlacePiece(piece, error);
  CHECK(id != kNoPiece, id == kNoPiece ? error.c_str() : "instance prototype is resident");
  if (id == kNoPiece) { return; }
  std::vector<float> atlas;
  CHECK(scene->Draw(error), "instance shadows submit");
  renderer.WaitForGpu();
  CHECK(renderer.ReadShadowAtlas(atlas) == ReadState::Ready && CastAt(atlas, -80) &&
            CastAt(atlas, 80) && !CastAt(atlas, 0),
        "both placements cast depth, with no unplaced prototype between them");
  rows[0][12] = -120;
  rows[1][12] = 120;
  CHECK(renderer.SetPieceInstances(id, rows, error) && scene->Draw(error),
        "moving instances invalidates the cached shadow atlas");
  renderer.WaitForGpu();
  CHECK(renderer.ReadShadowAtlas(atlas) == ReadState::Ready && CastAt(atlas, -120) &&
            CastAt(atlas, 120) && !CastAt(atlas, -80) && !CastAt(atlas, 80),
        "every caster moves and clears its old footprint");
  CHECK(renderer.SetPieceInstances(id, {}, error) && scene->Draw(error),
        "removing instances invalidates their shadows");
  renderer.WaitForGpu();
  CHECK(renderer.ReadShadowAtlas(atlas) == ReadState::Ready && !CastAt(atlas, -120) &&
            !CastAt(atlas, 120),
        "removed placements leave no ghost depth");
}
}

int main() {
  using namespace outshine::Test;
  CHECK(SDL_Init(SDL_INIT_VIDEO), "video initializes for instance shadows");
  CheckInstances(false);
  CheckInstances(true);
  SDL_Quit();
  return Report();
}
