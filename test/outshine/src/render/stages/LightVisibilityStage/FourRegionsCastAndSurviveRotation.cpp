#include "Check.h"
#include "RuntimeScene.h"
#include "SceneRenderer.h"
#include <SDL3/SDL.h>
#include <array>
#include <memory>
#include <string>
#include <vector>

namespace {
bool Cube(outshine::Geometry &geometry, float x) {
  const auto part = geometry.addPart("caster", outshine::MaterialInstance{});
  if (!part) { return false; }
  std::array<float, 24> vertices{-20, -20, -20, 20, -20, -20, 20, 20, -20, -20, 20, -20,
                                 -20, -20, 20,  20, -20, 20,  20, 20, 20,  -20, 20, 20};
  for (size_t at = 0; at < vertices.size(); at += 3) { vertices[at] += x; }
  return geometry.setPositions(*part, vertices) &&
         geometry.setTriangles(*part, std::array<uint32_t, 36>{0, 2, 1, 0, 3, 2, 4, 5, 6, 4, 6, 7,
                                                               0, 1, 5, 0, 5, 4, 3, 7, 6, 3, 6, 2,
                                                               0, 4, 7, 0, 7, 3, 1, 2, 6, 1, 6, 5});
}

bool Occupied(const std::vector<float> &atlas, size_t region, bool leftQuarter = false) {
  const size_t firstX = (region % 2) * outshine::Render::kShadowTilePx;
  const size_t firstY = (region / 2) * outshine::Render::kShadowTilePx;
  const size_t width = outshine::Render::kShadowTilePx / (leftQuarter ? 4 : 1);
  if (atlas.size() != size_t(outshine::Render::kShadowAtlasPx) * outshine::Render::kShadowAtlasPx) {
    return false;
  }
  for (size_t y = firstY; y < firstY + outshine::Render::kShadowTilePx; ++y) {
    for (size_t x = firstX; x < firstX + width; ++x) {
      if (atlas[y * outshine::Render::kShadowAtlasPx + x] > 0.0f) { return true; }
    }
  }
  return false;
}
}

int main() {
  using namespace outshine;
  using namespace outshine::Render;
  using namespace outshine::Test;
  CHECK(SDL_Init(SDL_INIT_VIDEO), "video initializes for regional shadow rendering");
  {
    Geometry geometry;
    CHECK(Cube(geometry, 0) && Cube(geometry, 6000), "near and distant closed casters are valid");
    Core::Declaration declaration;
    declaration.InitialGeometry = &geometry;
    declaration.SurfaceWidthPx = declaration.SurfaceHeightPx = 32;
    declaration.Outputs = {"surface", "sceneLinear", "shadowAtlas"};
    declaration.DrawsSky = true;
    declaration.ShadowRadiusM = 8192;
    declaration.KeyLux = 10000;
    declaration.KeyElevationDeg = 45;
    SceneRenderer renderer;
    std::unique_ptr<Core::RuntimeScene> scene;
    std::string error;
    CHECK(Core::RuntimeScene::Open(renderer, declaration, nullptr, scene, error),
          "caster world opens");
    if (scene) {
      Viewpoint eye;
      eye.EyeM = {{0, 0, 100}};
      eye.YfovRad = 1;
      eye.ZNearM = 0.1;
      eye.ZFarM = 20000;
      scene->Eye(eye);
      renderer.SetShadowFrame({{0, 0, 1}}, {{0, 1, 0}}, 8192, true);
      CHECK(scene->Draw(error), "four regional shadow passes submit");
      renderer.WaitForGpu();
      std::vector<float> first;
      CHECK(renderer.ReadShadowAtlas(first) == ReadState::Ready, "the depth atlas is readable");
      for (size_t region = 0; region < 4; ++region) {
        CHECK(Occupied(first, region), "each independent atlas tile contains its caster depths");
      }
      CHECK(Occupied(first, 3, true), "the far tile retains the six-kilometre offscreen caster");
      eye.Forward = {{1, 0, 0}};
      eye.Right = {{0, 0, 1}};
      scene->Eye(eye);
      CHECK(scene->Draw(error), "camera rotation renders without changing world coverage");
      renderer.WaitForGpu();
      std::vector<float> second;
      CHECK(renderer.ReadShadowAtlas(second) == ReadState::Ready && first == second,
            "rotation preserves every submitted depth texel");
    }
  }
  SDL_Quit();
  return Report();
}
