#include "RuntimeScene.h"
#include "SceneRenderer.h"
#include "WorldCandidate.h"
#include "Check.h"

#include <SDL3/SDL.h>
#include <array>
#include <memory>
#include <vector>

int main() {
  using namespace outshine;
  using namespace outshine::Test;
  CHECK(SDL_Init(SDL_INIT_VIDEO), "video initializes");
  {
    Geometry base;
    const auto surface = base.addSurface("wall", Material{});
    CHECK(surface.has_value(), "fixture owns a native material");
    if (!surface) { return Report(); }
    const auto part = base.addPart("offscreen", *surface);
    CHECK(part && base.setPositions(*part, std::array<float, 9>{100, 0, 0, 101, 0, 0, 100, 1, 0}) &&
              base.setTriangles(*part, std::array<uint32_t, 3>{0, 1, 2}),
          "offscreen base keeps a material slot without covering the probe");
    Core::Declaration declaration;
    declaration.InitialGeometry = &base;
    declaration.SurfaceWidthPx = declaration.SurfaceHeightPx = 32;
    declaration.Outputs = {"sceneDepth"};
    Render::SceneRenderer renderer;
    std::unique_ptr<Core::RuntimeScene> scene;
    std::string error;
    CHECK(Core::RuntimeScene::Open(renderer, declaration, nullptr, scene, error), "world opens");
    if (scene) {
      Render::Viewpoint eye;
      eye.EyeM = {{0, 0, 4}};
      eye.Kind = Render::CameraKind::Orthographic;
      eye.XMagM = eye.YMagM = 2;
      eye.ZNearM = 0.1;
      eye.ZFarM = 10;
      scene->Eye(eye);
      renderer.SetNativePieceSurfaces(std::array<uint32_t, 1>{0});
      const std::array<StoredVertex, 3> vertices{
          StoredVertex::Of({{-0.5f, -0.5f, 0}}, {{0, 0}}, {{0, 0, 1}}),
          StoredVertex::Of({{0.5f, -0.5f, 0}}, {{0, 0}}, {{0, 0, 1}}),
          StoredVertex::Of({{0, 0.5f, 0}}, {{0, 0}}, {{0, 0, 1}})};
      const std::array<uint32_t, 3> indices{0, 1, 2};
      const auto piece = renderer.PlaceStructurePiece({.Verts = vertices, .Indices = indices});
      CHECK(piece.has_value(), "structure piece becomes resident");
      if (piece) {
        const auto centreDepth = [&]() {
          std::vector<float> depth;
          if (!scene->Draw(error)) { return -1.0f; }
          renderer.WaitForGpu();
          if (renderer.ReadDepth(depth) != Render::ReadState::Ready || depth.size() != 32u * 32u) {
            return -1.0f;
          }
          return depth[16u * 32u + 16u];
        };
        CHECK(centreDepth() > 0.0f, "default single placement draws before publication");
        CHECK(renderer.SetPieceInstances(*piece, {}, error) && centreDepth() == 0.0f,
              "empty instances hide the piece before publication");
        Core::WorldCandidate candidate(renderer);
        const auto prepared = candidate.Prepare(*scene, nullptr);
        CHECK(prepared.has_value(), "candidate restores the hidden piece source");
        if (prepared) {
          CHECK(candidate.Publish(scene).has_value(), "candidate publishes its restored resources");
          scene->Eye(eye);
          CHECK(scene->Advance(error), "published camera updates its new renderer state");
          CHECK(centreDepth() == 0.0f && renderer.HasPieceSource(*piece),
                "hidden structure stays hidden with a valid handle after publication");
          const std::array<Mat4, 1> visible{};
          const bool revealed = renderer.SetPieceInstances(*piece, visible, error);
          CHECK(revealed,
                error.empty() ? "the restored handle accepts a visible instance" : error.c_str());
          const float restoredDepth = centreDepth();
          CHECK(restoredDepth > 0.0f, "the same restored handle can reveal the piece again");
        }
      }
    }
  }
  SDL_Quit();
  return Report();
}
