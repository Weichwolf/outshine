#include "Check.h"
#include "GpuSubmission.h"
#include "RuntimeScene.h"
#include "SceneRenderer.h"

#include <SDL3/SDL.h>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <print>
#include <span>
#include <string>
#include <vector>

namespace {
using namespace outshine;
using namespace outshine::Render;
using namespace outshine::Test;
constexpr size_t kPixels = 64;
constexpr double kNearM = 0.1;
constexpr float kTolerance = 0.001f;
constexpr float kDiagonalNormal = 0.7071067811865475f;
constexpr std::array<Vec3f, 4> kCorners{Vec3f{{-0.5f, -0.5f, 0.5f}},
                                        Vec3f{{0.5f, -0.5f, -0.5f}},
                                        Vec3f{{0.5f, 0.5f, -0.5f}},
                                        Vec3f{{-0.5f, 0.5f, 0.5f}}};

void CheckPlane(Core::RuntimeScene &scene,
                SceneRenderer &renderer,
                PieceHandle piece,
                const Mat4 &model,
                std::string &error) {
  CHECK(renderer.SetPieceInstances(piece, std::span(&model, 1), error),
        "the real instance accepts its affine placement");
  CHECK(scene.Draw(error), "the native surface frame submits");
  renderer.WaitForGpu();
  std::vector<float> depth;
  std::vector<float> normal;
  CHECK(renderer.ReadDepth(depth) == ReadState::Ready && depth.size() == kPixels * kPixels,
        "native depth covers the declared raster");
  CHECK(renderer.ReadShadingNormal(normal) == ReadState::Ready &&
            normal.size() == kPixels * kPixels * 4,
        "the actual shading-normal attachment is readable");
  if (depth.size() != kPixels * kPixels || normal.size() != kPixels * kPixels * 4) { return; }
  constexpr size_t probe = (kPixels / 2) * kPixels + kPixels / 2;
  CHECK(depth[probe] > 0, "the transformed plane covers the independent normal probe");
  const Vec3 first = model.TransformPoint(Vec3{{-0.5, -0.5, 0.5}});
  const Vec3 second = model.TransformPoint(Vec3{{0.5, -0.5, -0.5}});
  const Vec3 third = model.TransformPoint(Vec3{{0.5, 0.5, -0.5}});
  Vec3 expected = Cross(second - first, third - first);
  CHECK(Normalise(expected), "transformed geometric edges define a nonzero plane");
  std::println("normal expected=({}, {}, {}) actual=({}, {}, {})",
               expected[0],
               expected[1],
               expected[2],
               normal[4 * probe],
               normal[4 * probe + 1],
               normal[4 * probe + 2]);
  for (size_t axis = 0; axis < 3; ++axis) {
    CHECK(std::abs(normal[4 * probe + axis] - expected[axis]) < kTolerance,
          "native shading normal agrees with the independently transformed geometric plane");
  }
}

void CheckInstances() {
  GpuSubmission submission;
  submission.Create = [](void *, Native::DeviceOptions options) {
    options.Validation = true;
    return Native::CreateDevice(options);
  };
  SceneRenderer renderer{submission};
  std::unique_ptr<Core::RuntimeScene> scene;
  std::string error;
  Geometry base;
  const auto material = base.addSurface("affine-plane", Material{});
  CHECK(material, "the plane owns its material");
  if (!material) { return; }
  const auto part = base.addPart("outside-view", *material);
  constexpr std::array<float, 9> outside{100, 0, 0, 101, 0, 0, 100, 1, 0};
  constexpr std::array<uint32_t, 3> triangle{0, 1, 2};
  CHECK(part && base.setPositions(*part, outside) && base.setTriangles(*part, triangle),
        "the base scene has valid geometry outside the probe");
  Core::Declaration declaration;
  declaration.InitialGeometry = &base;
  declaration.SurfaceWidthPx = declaration.SurfaceHeightPx = kPixels;
  declaration.Stages = {"opaqueGeometry"};
  declaration.Outputs = {"sceneDepth", "sceneShadingNormal"};
  CHECK(Core::RuntimeScene::Open(renderer, declaration, nullptr, scene, error),
        "the real native surface scene opens");
  if (!scene) { return; }
  Viewpoint eye;
  eye.EyeM = {{0, 0, 4}};
  eye.Kind = CameraKind::Orthographic;
  eye.XMagM = eye.YMagM = 2;
  eye.ZNearM = kNearM;
  eye.ZFarM = 10;
  scene->Eye(eye);
  renderer.SetNativePieceSurfaces(std::array<uint32_t, 1>{0});
  std::array<StoredVertex, 4> vertices;
  for (size_t at = 0; at < vertices.size(); ++at) {
    vertices[at] =
        StoredVertex::Of(kCorners[at], {{0, 0}}, {{kDiagonalNormal, 0, kDiagonalNormal}});
  }
  constexpr std::array<uint32_t, 6> indices{0, 1, 2, 0, 2, 3};
  PieceMesh mesh;
  mesh.Verts = vertices;
  mesh.Indices = indices;
  mesh.MaxInstances = 1;
  const auto piece = renderer.PlacePiece(mesh);
  CHECK(piece, "the original sloping plane enters native residency");
  if (!piece) { return; }
  CheckPlane(*scene, renderer, *piece, Mat4{}, error);
  Mat4 scale;
  scale[0] = 2;
  CheckPlane(*scene, renderer, *piece, scale, error);
  Mat4 shear;
  shear[8] = 0.5;
  CheckPlane(*scene, renderer, *piece, shear, error);
}
}

int main() {
  CHECK(SDL_Init(SDL_INIT_VIDEO), "video initializes for native affine normals");
  CheckInstances();
  SDL_Quit();
  return Report();
}
