#include <array>
#include <cmath>
#include <cstdio>
#include <memory>
#include <SDL3/SDL.h>
#include "Live.h"
#include "Check.h"

int main() {
  using namespace outshine;
  using namespace outshine::Test;
  if (!SDL_Init(SDL_INIT_VIDEO)) { Unprepared(SDL_GetError()); return Report(); }
  std::array<double, 3> measured{};
  for (size_t at = 0; at < measured.size(); ++at) {
    Render::SceneRenderer renderer;
    Core::Declaration declaration;
    declaration.SurfaceWidthPx = declaration.SurfaceHeightPx = 16;
    declaration.Haze = static_cast<double>(at);
    declaration.DrawsSky = true;
    declaration.KeyFromClock = true;
    declaration.KeyElevationDeg = 45;
    std::unique_ptr<Core::Live> scene;
    std::string error;
    if (!Core::Live::Open(renderer, declaration, nullptr, scene, error)) {
      Unprepared(error.c_str());
      return Report();
    }
    measured[at] = scene->MeteredLux();
    std::printf("haze %.0f metered %.9f lux\n", declaration.Haze, measured[at]);
    CHECK(measured[at] > 0 && std::isfinite(measured[at]), "daylight meters a finite positive illuminance");
    CHECK_NEAR(scene->MeteredLux(), measured[at], 0.0, "lux", "unchanged air and sun reuse the same result");
  }
  CHECK(std::abs(measured[0] - measured[2]) > 1.0,
        "changing the declared aerosol density changes the clocked daylight exposure");
  return Report();
}
