#include "Check.h"
#include "RuntimeScene.h"
#include "SceneRenderer.h"
#include "WorldCandidate.h"
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
    CHECK(part &&
              base.setPositions(
                  *part,
                  std::array<float, 18>{
                      -101, -1, 0, -100, -1, 0, -100, 1, 0, 100, -1, 0, 101, -1, 0, 100, 1, 0}) &&
              base.setTriangles(*part, std::array<uint32_t, 6>{0, 1, 2, 3, 4, 5}),
          "symmetric off-view base preserves a zero scene anchor without covering the probe");
    Core::Declaration declaration;
    declaration.InitialGeometry = &base;
    declaration.SurfaceWidthPx = declaration.SurfaceHeightPx = 32;
    declaration.Outputs = {"sceneDepth"};
    Render::SceneRenderer renderer;
    std::unique_ptr<Core::RuntimeScene> scene;
    std::string error;
    CHECK(Core::RuntimeScene::Open(renderer, declaration, nullptr, scene, error), "world opens");
    if (!scene) { return Report(); }
    Render::Viewpoint eye;
    eye.EyeM = {{0, 0, 4}};
    eye.Kind = Render::CameraKind::Orthographic;
    eye.XMagM = eye.YMagM = 2;
    eye.ZNearM = 0.1;
    eye.ZFarM = 10;
    scene->Eye(eye);
    renderer.SetNativePieceSurfaces(std::array<uint32_t, 1>{0});
    std::array<StoredVertex, 3> vertices{
        StoredVertex::Of({{-0.5f, -0.5f, 0}}, {{0, 0}}, {{0, 0, 1}}),
        StoredVertex::Of({{0.5f, -0.5f, 0}}, {{1, 0}}, {{0, 0, 1}}),
        StoredVertex::Of({{0, 0.5f, 0}}, {{0, 1}}, {{0, 0, 1}})};
    const std::array<uint32_t, 3> indices{0, 1, 2};
    const auto piece = renderer.PlaceStructurePiece({.Verts = vertices, .Indices = indices});
    CHECK(piece.has_value(), "static piece becomes resident");
    if (!piece) { return Report(); }
    const auto depthAtCentre = [&]() {
      std::vector<float> depth;
      if (!scene->Draw(error)) { return -1.0f; }
      renderer.WaitForGpu();
      if (renderer.ReadDepth(depth) != Render::ReadState::Ready || depth.size() != 32u * 32u) {
        return -1.0f;
      }
      return depth[16u * 32u + 16u];
    };
    CHECK(depthAtCentre() > 0, "visible piece covers the depth probe");
    const auto visibleCalls = renderer.SubjectDrawCallCount();
    CHECK(visibleCalls > 0, "probe has recorded subject work");
    std::array<Mat4, 1> rows{};
    rows[0].SetTranslation({{100, 0, 0}});
    CHECK(renderer.SetPieceInstances(*piece, rows, error) && depthAtCentre() == 0 &&
              renderer.SubjectDrawCallCount() + 1 == visibleCalls,
          "off-view placement removes its draw without removing residency");
    Core::WorldCandidate candidate(renderer);
    const auto prepared = candidate.Prepare(*scene, nullptr);
    CHECK(prepared.has_value(), "off-view source survives resource recreation");
    if (!prepared) { return Report(); }
    CHECK(candidate.Publish(scene).has_value(), "restored world publishes");
    scene->Eye(eye);
    CHECK(scene->Advance(error) && depthAtCentre() == 0 &&
              renderer.SubjectDrawCallCount() + 1 == visibleCalls,
          "restored off-view batch is still rejected");
    rows[0] = {};
    CHECK(renderer.SetPieceInstances(*piece, rows, error) && depthAtCentre() > 0 &&
              renderer.SubjectDrawCallCount() == visibleCalls,
          "moving back refreshes its bound and restores the draw");
    renderer.ReleasePiece(*piece);
    for (auto &vertex : vertices) { vertex.pos[0] += 100; }
    const auto reused = renderer.PlaceStructurePiece({.Verts = vertices, .Indices = indices});
    CHECK(reused && reused->Slot == piece->Slot, "new geometry reuses the released source slot");
    if (!reused) { return Report(); }
    rows[0].SetTranslation({{-100, 0, 0}});
    CHECK(renderer.SetPieceInstances(*reused, rows, error) && depthAtCentre() > 0 &&
              renderer.SubjectDrawCallCount() == visibleCalls,
          "reused slot gets the new local bound rather than its predecessor's bound");
  }
  SDL_Quit();
  return Report();
}
