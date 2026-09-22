#include "RuntimeScene.h"
#include "WorldCandidate.h"
#include "Check.h"

#include <SDL3/SDL.h>
#include <array>
#include <memory>
#include <string>

int main() {
  using namespace outshine;
  using namespace outshine::Test;
  if (!SDL_Init(SDL_INIT_VIDEO)) {
    Unprepared(SDL_GetError());
    return Report();
  }
  {
    Geometry geometry;
    const auto surface = geometry.addSurface("surface", Material{});
    CHECK(surface.has_value(), "native material allocates");
    if (!surface) { return Report(); }
    const auto part = geometry.addPart("triangle", *surface);
    CHECK(part.has_value(), "native part allocates");
    if (!part) { return Report(); }
    CHECK(geometry.setPositions(*part, std::array<float, 9>{0, 0, 0, 1, 0, 0, 0, 1, 0}),
          "native positions stand");
    CHECK(geometry.setTriangles(*part, std::array<uint32_t, 3>{0, 1, 2}), "native indices stand");
    for (bool withGeometry : {false, true}) {
      Render::SceneRenderer renderer;
      Core::Declaration declaration;
      declaration.SurfaceWidthPx = declaration.SurfaceHeightPx = 16;
      declaration.KeyFromClock = true;
      declaration.KeyElevationDeg = 45;
      declaration.InitialGeometry = withGeometry ? &geometry : nullptr;
      std::unique_ptr<Core::RuntimeScene> scene;
      std::string error;
      std::array<double, 2> light{};
      for (size_t state = 0; state < light.size(); ++state) {
        declaration.Haze = static_cast<double>(state);
        CHECK(Core::RuntimeScene::Open(renderer, declaration, nullptr, scene, error),
              "declared atmosphere opens");
        if (!scene) { return Report(); }
        light[state] = scene->MeteredLux();
        CHECK(scene->SkyIntegrations() == state + 1, "changed air integrates once");
      }
      CHECK(light[0] != light[1], "changed air changes metered light");
      {
        Core::WorldCandidate rejected(renderer);
        CHECK(rejected.Prepare(*scene, nullptr).has_value(), "discarded replacement prepares");
      }
      CHECK(scene->SkyIntegrations() == 2 && scene->MeteredLux() == light[1],
            "abandonment preserves published light and evaluation history");
      for (int replacement = 0; replacement < 3; ++replacement) {
        Core::WorldCandidate candidate(renderer);
        const auto prepared = candidate.Prepare(*scene, nullptr);
        CHECK(prepared.has_value(), "replacement prepares");
        if (!prepared) { return Report(); }
        CHECK(candidate.Publish(scene).has_value(), "replacement publishes");
        CHECK(scene->SkyIntegrations() == 2,
              "replacements preserve the populated cache before building");
        CHECK(scene->MeteredLux() == light[1] && scene->SkyIntegrations() == 2,
              "unchanged metering returns exact light without integration");
      }
      declaration.KeyElevationDeg = 30;
      CHECK(Core::RuntimeScene::Open(renderer, declaration, nullptr, scene, error),
            "changed sun opens");
      CHECK(scene->SkyIntegrations() == 3, "changed sun invalidates inherited light");
      CHECK(scene->MeteredLux() != light[1], "changed sun changes metered light");
    }
  }
  SDL_Quit();
  return Report();
}
