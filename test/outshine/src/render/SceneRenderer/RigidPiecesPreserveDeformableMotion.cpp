#include "Check.h"
#include "RuntimeScene.h"
#include "SceneRenderer.h"

#include <SDL3/SDL.h>
#include <array>
#include <cmath>
#include <cstdio>
#include <memory>
#include <string>
#include <vector>

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
      eye.ZNearM = 0.1;
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
      CHECK(renderer.ReadDepth(depth) == ReadState::Ready && depth.size() == 64u * 64u,
            "depth proves both probes contain geometry");
      CHECK(renderer.ReadSceneVelocity(velocity) == ReadState::Ready &&
                velocity.size() == 64u * 64u * 2u,
            "motion attachment is readable");
      if (depth.size() == 64u * 64u && velocity.size() == 64u * 64u * 2u) {
        constexpr size_t left = 32u * 64u + 16u;
        constexpr size_t right = 32u * 64u + 48u;
        std::fprintf(stderr,
                     "left depth=%g velocity=(%g,%g); right depth=%g velocity=(%g,%g)\n",
                     depth[left],
                     velocity[2u * left],
                     velocity[2u * left + 1u],
                     depth[right],
                     velocity[2u * right],
                     velocity[2u * right + 1u]);
        CHECK(depth[left] > 0 && depth[right] > 0, "both triangles cover their probes");
        CHECK(std::abs(velocity[2u * left] - 0.25f / 2.0f) < 0.001f &&
                  std::abs(velocity[2u * left + 1u]) < 0.001f,
              "quarter-metre deformation yields displacement divided by orthographic half-width");
        CHECK(std::abs(velocity[2u * right]) < 0.001f &&
                  std::abs(velocity[2u * right + 1u]) < 0.001f,
              "stationary rigid geometry has zero motion after the deforming draw");
      }
    }
  }
  SDL_Quit();
  return Report();
}
