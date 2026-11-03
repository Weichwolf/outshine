#include "Check.h"
#include "RuntimeScene.h"
#include "SceneRenderer.h"
#include "Geometry.h"
#include "Material.h"
#include "StoredVertex.h"
#include <SDL3/SDL.h>
#include <algorithm>
#include <array>
#include <memory>
#include <span>
#include <string>
#include <vector>

namespace {
using namespace outshine;
using namespace outshine::Render;
using namespace outshine::Test;
constexpr Mat4 kIdentity{{1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1}};

bool Quad(Geometry &geometry, const char *name, Material material, float half, float depth) {
  const auto surface = geometry.addSurface(name, material);
  if (!surface) { return false; }
  const auto part = geometry.addPart(name, *surface);
  return part &&
         geometry.setPositions(
             *part,
             std::array<float, 12>{
                 -half, -half, depth, half, -half, depth, half, half, depth, -half, half, depth}) &&
         geometry.setTriangles(*part, std::array<uint32_t, 6>{0, 1, 2, 0, 2, 3});
}

PieceId Mover(SceneRenderer &renderer, std::string &error) {
  constexpr std::array<Vec3f, 3> points{
      {{{1.1f, -0.3f, -4}}, {{1.7f, -0.3f, -4}}, {{1.4f, 0.3f, -4}}}};
  std::array<StoredVertex, 3> vertices;
  for (size_t at = 0; at < vertices.size(); ++at) {
    vertices[at] = StoredVertex::Of(points[at], {{0, 0}}, {{0, 0, 1}});
  }
  constexpr std::array<uint32_t, 3> indices{0, 1, 2};
  const std::array<DagCluster, 1> clusters{{{.SelfCenter = {{1.4f, 0, -4}},
                                             .SelfRadius = 0.5f,
                                             .ParentCenter = {{1.4f, 0, -4}},
                                             .ParentRadius = 0.5f,
                                             .ParentErr = kDagRootErr,
                                             .Count = indices.size()}}};
  const std::array<Mat4, 1> rows{kIdentity};
  return renderer.PlacePiece({.Verts = vertices,
                              .Indices = indices,
                              .Clusters = clusters,
                              .Instances = rows,
                              .MaxInstances = 1,
                              .Surface = PieceSurface(2)},
                             error);
}

void Exercise() {
  Geometry geometry;
  Material green;
  green.Unlit = true;
  green.Alpha = AlphaMode::Blended;
  green.BaseColour = {{0, 1, 0, 0.5f}};
  Material grey;
  grey.Unlit = true;
  grey.BaseColour = {{0.1f, 0.1f, 0.1f, 1}};
  Material red;
  red.Unlit = true;
  red.BaseColour = {{1, 0, 0, 1}};
  CHECK(Quad(geometry, "blend-first-in-storage", green, 3, -6) &&
            Quad(geometry, "near-opaque", grey, 0.75f, -2) &&
            Quad(geometry, "hidden-red", red, 0.3f, -12),
        "a real translucent surface precedes opaque surfaces in source storage");
  Core::Declaration declaration;
  declaration.InitialGeometry = &geometry;
  declaration.SurfaceWidthPx = declaration.SurfaceHeightPx = 128;
  declaration.Stages = {"opaqueGeometry", "depthPyramid"};
  declaration.Outputs = {"sceneLinear", "sceneDepth"};
  SceneRenderer history, reference;
  std::unique_ptr<Core::RuntimeScene> a, b;
  std::string error;
  CHECK(Core::RuntimeScene::Open(history, declaration, nullptr, a, error) &&
            Core::RuntimeScene::Open(reference, declaration, nullptr, b, error),
        "history and independent fresh-pose reference use identical inputs");
  if (!a || !b) { return; }
  const auto moverA = Mover(history, error), moverB = Mover(reference, error);
  CHECK(moverA != kNoPiece && moverB != kNoPiece, "both scenes contain a dynamic instance");
  Viewpoint eye;
  eye.YfovRad = 1;
  eye.ZNearM = 0.1;
  eye.ZFarM = 100;
  a->Eye(eye);
  CHECK(a->Draw(error) && a->Draw(error), "the near occluder creates genuine old depth");
  eye.EyeM[0] = 2;
  a->Eye(eye);
  b->Eye(eye);
  CHECK(a->Draw(error) && b->Draw(error), "camera movement exposes the target behind the blend");
  std::vector<float> actual, expected;
  CHECK(history.ReadSceneLinear(actual) == ReadState::Ready &&
            reference.ReadSceneLinear(expected) == ReadState::Ready && actual == expected,
        "deferred opaque recovery precedes translucent compositing");
  bool mixed = false;
  for (size_t at = 0; at + 3 < actual.size(); at += 4) {
    mixed |= actual[at] > 0.3f && actual[at + 1] > 0.3f;
  }
  CHECK(mixed, "the comparison contains actual red/green alpha compositing");
  const auto before = actual;
  Mat4 shifted = kIdentity;
  shifted[12] = 1;
  const std::array<Mat4, 1> rows{shifted};
  CHECK(history.SetPieceInstances(moverA, rows, error) &&
            reference.SetPieceInstances(moverB, rows, error) && a->Draw(error) && b->Draw(error),
        "current instance transforms change without replacing the scene");
  CHECK(history.ReadSceneLinear(actual) == ReadState::Ready &&
            reference.ReadSceneLinear(expected) == ReadState::Ready && actual == expected &&
            actual != before,
        "changed instance geometry survives both phases and produces the reference image");
  history.ReleasePiece(moverA);
  reference.ReleasePiece(moverB);
  CHECK(a->Draw(error) && b->Draw(error),
        "both scenes remain renderable after instance retirement");
  CHECK(history.ReadSceneLinear(actual) == ReadState::Ready &&
            reference.ReadSceneLinear(expected) == ReadState::Ready && actual == expected,
        "retired slots cannot leak old first-phase work into recovery");
}
}

int main() {
  CHECK(SDL_Init(SDL_INIT_VIDEO), "video initializes");
  Exercise();
  SDL_Quit();
  return Report();
}
