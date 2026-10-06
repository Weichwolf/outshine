#include "Check.h"
#include "RuntimeScene.h"
#include "SceneRenderer.h"

#include <SDL3/SDL.h>
#include <array>
#include <cmath>
#include <cstdio>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <print>
#include <string>
#include <vector>

namespace {
constexpr size_t kResolution = 64;
constexpr double kNearPlaneM = 0.1;
constexpr float kDeformationM = 0.25f;
constexpr float kTolerance = 0.001f;

void CheckMotion(outshine::Render::SceneRenderer &renderer, float expected) {
  using namespace outshine::Render;
  using namespace outshine::Test;
  renderer.WaitForGpu();
  std::vector<float> depth;
  std::vector<float> velocity;
  constexpr size_t wide = kResolution;
  constexpr size_t left = 32u * wide + 16u;
  constexpr size_t right = 32u * wide + 48u;
  CHECK(renderer.ReadDepth(depth) == ReadState::Ready && depth.size() == wide * wide,
        "both pose-transition probes retain depth");
  CHECK(renderer.ReadSceneVelocity(velocity) == ReadState::Ready &&
            velocity.size() == wide * wide * 2u,
        "pose-transition velocities are readable");
  if (depth.size() != wide * wide || velocity.size() != wide * wide * 2u) { return; }
  CHECK(depth[left] > 0 && depth[right] > 0, "pose transitions preserve both covered triangles");
  CHECK(std::abs(velocity[2u * left] - expected) < kTolerance &&
            std::abs(velocity[2u * left + 1u]) < kTolerance,
        "main mesh motion follows the supplied previous pose");
  CHECK(std::abs(velocity[2u * right]) < kTolerance &&
            std::abs(velocity[2u * right + 1u]) < kTolerance,
        "native rigid geometry retains zero deformation motion");
}
}

int main() {
  using namespace outshine;
  using namespace outshine::Render;
  using namespace outshine::Test;
  CHECK(SDL_Init(SDL_INIT_VIDEO), "video initializes");
  {
    Geometry base;
    const auto surface = base.addSurface("wall", Material{});
    CHECK(surface.has_value(), "material is available");
    if (!surface) { return Report(); }
    const auto part = base.addPart("subject", *surface);
    constexpr std::array<float, 9> positions{-1.5f, -0.5f, 0, -0.5f, -0.5f, 0, -1, 0.5f, 0};
    constexpr std::array<float, 9> previous{-1.75f, -0.5f, 0, -0.75f, -0.5f, 0, -1.25f, 0.5f, 0};
    constexpr std::array<float, 9> normals{0, 0, 1, 0, 0, 1, 0, 0, 1};
    constexpr std::array<float, 9> emitted{};
    constexpr std::array<uint32_t, 3> indices{0, 1, 2};
    CHECK(part && base.setPositions(*part, positions) && base.setTriangles(*part, indices),
          "subject geometry is complete");
    Core::Declaration declaration;
    declaration.InitialGeometry = &base;
    declaration.SurfaceWidthPx = declaration.SurfaceHeightPx = 64;
    declaration.Stages = {"subjects"};
    declaration.Outputs = {"sceneDepth", "sceneVelocity"};
    SceneRenderer renderer;
    std::unique_ptr<Core::RuntimeScene> scene;
    std::string error;
    CHECK(Core::RuntimeScene::Open(renderer, declaration, nullptr, scene, error), "scene opens");
    if (scene) {
      Viewpoint eye;
      eye.EyeM = {{0, 0, 4}};
      eye.Kind = CameraKind::Orthographic;
      eye.XMagM = eye.YMagM = 2;
      eye.ZNearM = kNearPlaneM;
      eye.ZFarM = 10;
      scene->Eye(eye);
      CHECK(scene->Draw(error), "first frame establishes the previous camera");
      DrawList draws;
      CHECK(draws.Add({.IndexCount = 3, .Layout = VertexLayout::PositionNormal}, error),
            "deforming draw uses the rigid piece's pipeline layout");
      draws.Compile();
      SubjectMesh mesh;
      mesh.Anchor = renderer.LastSubmittedCamera().Basis.EyeM - eye.EyeM;
      mesh.Verts.From = positions.data();
      mesh.Positions = positions;
      mesh.PrevVerts.From = previous.data();
      mesh.Normals.From = normals.data();
      mesh.Emitted.From = emitted.data();
      mesh.VertexCount = 3;
      mesh.Indices = indices.data();
      mesh.IndexCount = 3;
      mesh.Draws = &draws;
      CHECK(renderer.SetSubjectMesh(mesh, error), "distinct previous pose uploads");
      renderer.SetNativePieceSurfaces(std::array<uint32_t, 1>{0});
      const std::array<StoredVertex, 3> rigid{
          StoredVertex::Of({{0.5f, -0.5f, 0}}, {{0, 0}}, {{0, 0, 1}}),
          StoredVertex::Of({{1.5f, -0.5f, 0}}, {{0, 0}}, {{0, 0, 1}}),
          StoredVertex::Of({{1, 0.5f, 0}}, {{0, 0}}, {{0, 0, 1}})};
      CHECK(renderer.PlaceStructurePiece({.Verts = rigid, .Indices = indices}).has_value(),
            "rigid piece shares the arena");
      CHECK(scene->Draw(error), "mixed geometry renders");
      renderer.WaitForGpu();
      std::vector<float> depth;
      std::vector<float> velocity;
      CHECK(renderer.ReadDepth(depth) == ReadState::Ready &&
                depth.size() == kResolution * kResolution,
            "depth proves both probes contain geometry");
      CHECK(renderer.ReadSceneVelocity(velocity) == ReadState::Ready &&
                velocity.size() == kResolution * kResolution * 2u,
            "motion attachment is readable");
      if (depth.size() == kResolution * kResolution &&
          velocity.size() == kResolution * kResolution * 2u) {
        constexpr size_t left = 32u * 64u + 16u;
        constexpr size_t right = 32u * 64u + 48u;
        std::println(stderr,
                     "left depth={} velocity=({},{}); right depth={} velocity=({},{})",
                     depth[left],
                     velocity[2u * left],
                     velocity[2u * left + 1u],
                     depth[right],
                     velocity[2u * right],
                     velocity[2u * right + 1u]);
        CHECK(depth[left] > 0 && depth[right] > 0, "both triangles cover their probes");
        CHECK(std::abs(velocity[2u * left] - kDeformationM / 2.0f) < kTolerance &&
                  std::abs(velocity[2u * left + 1u]) < kTolerance,
              "quarter-metre deformation yields displacement divided by orthographic half-width");
        CHECK(std::abs(velocity[2u * right]) < kTolerance &&
                  std::abs(velocity[2u * right + 1u]) < kTolerance,
              "stationary rigid geometry has zero motion after the deforming draw");
      }
      for (const bool deforming : {false, true}) {
        SubjectPose pose = mesh;
        if (!deforming) { pose.PrevVerts = {}; }
        CHECK(renderer.SetSubjectPose(pose, error), "pose changes without replacing the mesh");
        CHECK(scene->Draw(error), "transitioned pose renders alongside rigid geometry");
        CheckMotion(renderer, deforming ? kDeformationM / 2.0f : 0.0f);
      }
    }
  }
  SDL_Quit();
  return Report();
}
