#include "RuntimeScene.h"
#include "Check.h"

#include <SDL3/SDL.h>
#include <array>
#include <cmath>
#include <memory>
#include <string>
#include <vector>

int main() {
  using namespace outshine;
  using namespace outshine::Test;
  if (!SDL_Init(SDL_INIT_VIDEO)) {
    Unprepared(SDL_GetError());
    return Report();
  }
  {
    Render::SceneRenderer renderer;
    Core::Declaration declaration;
    declaration.SurfaceWidthPx = declaration.SurfaceHeightPx = 32;
    declaration.Outputs = {"sceneLinear"};
    declaration.KeyLux = 10;
    declaration.KeyElevationDeg = 90;
    declaration.Haze = 0;
    std::unique_ptr<Core::RuntimeScene> scene;
    std::string error;
    if (!Core::RuntimeScene::Open(renderer, declaration, nullptr, scene, error)) {
      Unprepared(error.c_str());
      return Report();
    }
    Material material;
    material.BaseColour = {{0.2f, 0.2f, 0.2f, 1}};
    material.Roughness = 1;
    Geometry materials;
    CHECK(materials.addSurface("native-only", material).has_value(), "native material declared");
    const auto first = renderer.RegisterPieceMaterials(std::move(materials));
    CHECK(first.has_value(), "native material registers independently of primary geometry");
    if (!first) { return Report(); }
    const std::array<StoredVertex, 4> vertices{
        StoredVertex::Of({{-1, 0, 1}}, {{0, 0}}, {{0, 1, 0}}),
        StoredVertex::Of({{1, 0, 1}}, {{0, 0}}, {{0, 1, 0}}),
        StoredVertex::Of({{1, 0, -1}}, {{0, 0}}, {{0, 1, 0}}),
        StoredVertex::Of({{-1, 0, -1}}, {{0, 0}}, {{0, 1, 0}})};
    const std::array<uint32_t, 6> indices{0, 1, 2, 0, 2, 3};
    const auto piece =
        renderer.PlaceStructurePiece({.Verts = vertices,
                                      .Indices = indices,
                                      .Surface = Render::PieceSurface::Registered(*first)});
    CHECK(piece.has_value(), "the scene contains a native piece and no primary triangles");
    if (!piece) { return Report(); }
    Render::Viewpoint eye;
    eye.EyeM = {{0, 2, 0}};
    eye.Forward = {{0, -1, 0}};
    eye.Right = {{1, 0, 0}};
    eye.Up = {{0, 0, -1}};
    eye.Kind = Render::CameraKind::Orthographic;
    eye.XMagM = eye.YMagM = 2;
    eye.ZNearM = 0.1;
    eye.ZFarM = 10;
    scene->Eye(eye);
    for (int frame = 0; frame < 3; ++frame) {
      CHECK(scene->Advance(error) && scene->Draw(error), "native-only scene renders");
    }
    std::vector<float> pixels;
    CHECK(renderer.ReadSceneLinear(pixels) == Render::ReadState::Ready,
          "actual native HDR radiance is available");
    CHECK(pixels.size() == 32u * 32u * 4u, "complete native-only image is read");
    if (pixels.size() == 32u * 32u * 4u) {
      const size_t centre = (16u * 32u + 16u) * 4u;
      // Diffuse reference is 0.2 * 10 / pi = 0.637; broad bounds allow the declared dielectric
      // BRDF.
      CHECK(std::isfinite(pixels[centre]) && pixels[centre] > 0.3f && pixels[centre] < 2.0f,
            "ten lux lights the native dielectric independently of primary mesh presence");
      CHECK(std::abs(pixels[centre] - pixels[centre + 1u]) < 1e-5f &&
                std::abs(pixels[centre] - pixels[centre + 2u]) < 1e-5f,
            "neutral source and material preserve neutral native radiance");
    }
  }
  SDL_Quit();
  return Report();
}
