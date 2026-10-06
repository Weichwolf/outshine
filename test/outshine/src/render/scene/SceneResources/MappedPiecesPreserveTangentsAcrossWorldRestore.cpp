#include "RuntimeScene.h"
#include "SceneRenderer.h"
#include "WorldCandidate.h"
#include "Check.h"
#include <SDL3/SDL.h>
#include <array>
#include <cmath>
#include <memory>
#include <string>
#include <vector>

namespace {
using namespace outshine;

std::array<StoredVertex, 3> Triangle(float x) {
  return {StoredVertex::Of({{x - 0.6f, -0.7f, 0}}, {{0, 0}}, {{0, 0, 1}}),
          StoredVertex::Of({{x + 0.6f, -0.7f, 0}}, {{1, 0}}, {{0, 0, 1}}),
          StoredVertex::Of({{x, 0.7f, 0}}, {{0.5f, 1}}, {{0, 0, 1}})};
}

bool ReadNormals(Render::SceneRenderer &renderer, std::vector<float> &normal) {
  renderer.WaitForGpu();
  if (renderer.ReadShadingNormal(normal) != Render::ReadState::Ready ||
      normal.size() != 64 * 64 * 4) {
    return false;
  }
  const size_t left = (32 * 64 + 20) * 4;
  const size_t right = (32 * 64 + 44) * 4;
  return normal[left] > 0.70f && normal[right] < -0.70f && normal[left + 1] > 0.70f &&
         normal[right + 1] > 0.70f;
}
}

int main() {
  using namespace outshine;
  using namespace outshine::Test;
  CHECK(SDL_Init(SDL_INIT_VIDEO), "video initializes");
  {
    Geometry base;
    const auto surface = base.addSurface("offscreen", Material{});
    if (!surface) { return Report(); }
    const auto part = base.addPart("offscreen", *surface);
    CHECK(part && base.setPositions(*part, std::array<float, 9>{10, 0, 0, 11, 0, 0, 10, 1, 0}) &&
              base.setTriangles(*part, std::array<uint32_t, 3>{0, 1, 2}),
          "unmapped geometry precedes both tangent consumers");
    Core::Declaration declaration;
    declaration.InitialGeometry = &base;
    declaration.SurfaceWidthPx = declaration.SurfaceHeightPx = 64;
    declaration.Outputs = {"sceneShadingNormal", "sceneDepth"};
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
    Geometry materials;
    const auto image = materials.addImage(1, 1, std::array<uint8_t, 4>{218, 218, 128, 255});
    if (!image) { return Report(); }
    Material mapped;
    mapped.NormalMap.Image = *image;
    CHECK(materials.addSurface("mapped", mapped), "normal-mapped material is complete");
    const auto registered = renderer.RegisterPieceMaterials(std::move(materials));
    CHECK(registered, "normal map uploads");
    if (!registered) { return Report(); }
    const std::array<uint32_t, 3> indices{0, 1, 2};
    const std::array<float, 12> positive{1, 0, 0, 1, 1, 0, 0, 1, 1, 0, 0, 1};
    const std::array<float, 12> negative{-1, 0, 0, -1, -1, 0, 0, -1, -1, 0, 0, -1};
    const std::array<float, 12> colours{1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1};
    const auto left = Triangle(-0.75f);
    const auto right = Triangle(0.75f);
    const auto uncoloured =
        renderer.PlacePiece({.Tangents = positive,
                             .Verts = left,
                             .Indices = indices,
                             .Surface = Render::PieceSurface::Registered(*registered),
                             .Textured = true});
    const auto coloured =
        renderer.PlacePiece({.Tangents = negative,
                             .Verts = right,
                             .Indices = indices,
                             .Colours = colours,
                             .Surface = Render::PieceSurface::Registered(*registered),
                             .Textured = true});
    CHECK(uncoloured && coloured, "tinted and untinted tangent consumers both upload");
    CHECK(scene->Draw(error), "both storage-binding variants draw");
    std::vector<float> before;
    CHECK(ReadNormals(renderer, before),
          "GPU pixels preserve opposite tangent axes and handedness");
    Core::WorldCandidate candidate(renderer);
    const auto prepared = candidate.Prepare(*scene, nullptr);
    CHECK(prepared, "world replacement prepares");
    if (!prepared) { return Report(); }
    CHECK(candidate.SetGeometry(Geometry{}, 0, error), "main geometry is removed independently");
    const auto published = candidate.Publish(scene);
    CHECK(published, "native resources restore into the replacement world");
    if (!published) { return Report(); }
    scene->Eye(eye);
    CHECK(scene->Draw(error), "restored mapped pieces draw");
    std::vector<float> after;
    CHECK(ReadNormals(renderer, after) && before == after,
          "world restore preserves every sampled normal despite rebased vertex ranges");
  }
  SDL_Quit();
  return Report();
}
