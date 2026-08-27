#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <vector>

#include <Outshine.h>
#include <Scenario.h>

#include "Check.h"

namespace {

// THE SUN IS NOT A STAGE. It is the sky's own radiance in the direction of the sun, and a sky
// that integrates in-scattering while omitting the disc has left out the brightest thing in it.
//
// The radiance of the disc follows from the illuminance the scenario declares. A source of
// angular radius t subtends a solid angle
//
//   omega = pi * t^2
//
// and delivers E lux onto a surface facing it, so its radiance is E / omega -- and what reaches
// the eye is that, attenuated by the transmittance along the view ray, which is the same table
// the in-scattering already samples. The sun's angular radius is 4.6542e-3 rad, which is a
// measurement of the sky and not a number this tree chose.
//
// SO THE DISC IS ENORMOUS AGAINST THE SKY AROUND IT: at 40 000 lux the disc carries
// 40000 / (pi * 4.6542e-3^2) = 5.9e8, against a clear zenith of order 1e4. Six thousand times.
// No tonemapper turns that into the same pixel, and the case needs no tolerance -- it asks
// whether the brightest pixel in the frame is far above the sky beside it.
//
// THE CONTROL IS THE SAME SKY WITH THE SUN BEHIND THE CAMERA. Every other thing about the frame
// is identical: the same medium, the same illuminance, the same exposure. If the peak follows
// the sun's BEARING rather than its brightness, what is measured is a disc and not a brighter
// sky.
constexpr int kFramePx = 128;

[[nodiscard]] outshine::Scenario Under(double bearingDeg) {
  outshine::Scenario made;
  made.Render.Declared = true;
  made.Render.Frame = outshine::Extent{kFramePx, kFramePx};
  made.Ground.Declared = true;
  made.Ground.Lat = 48.1372;
  made.Ground.Lon = 11.5756;
  made.Lit.Declared = true;
  made.Lit.Key.Lux = 40000.0;
  made.Lit.Key.ElevationDeg = 20.0;
  made.Lit.Key.BearingDeg = bearingDeg;
  return made;
}

struct Read {
  int Peak = 0;
  double Mean = 0.0;
};

[[nodiscard]] Read Judged(const std::vector<uint8_t> &rgba) {
  Read said;
  const size_t pixels = rgba.size() / 4;
  if (pixels == 0) { return said; }
  double sum = 0.0;
  for (size_t at = 0; at < pixels; ++at) {
    const int r = rgba[at * 4], g = rgba[at * 4 + 1], b = rgba[at * 4 + 2];
    const int most = r > g ? (r > b ? r : b) : (g > b ? g : b);
    sum += most;
    if (most > said.Peak) { said.Peak = most; }
  }
  said.Mean = sum / (double)pixels;
  return said;
}

}

int main(void) {
  using namespace outshine::Test;
  std::setvbuf(stdout, nullptr, _IONBF, 0);

  if (!SDL_Init(SDL_INIT_VIDEO)) {
    Unprepared("SDL did not start, so nothing can be rendered");
    return Report();
  }

  outshine::Engine engine;
  engine.Under(outshine::Roots{".", "src/assets", "/tmp/outshine-door-cache", true});
  if (!engine.DrawsInto(outshine::Extent{kFramePx, kFramePx})) {
    Unprepared("the device stood no canvas");
    return Report();
  }

  int brightest = -1;
  double atBearing = 0.0;
  Read best;
  Read dimmest{1 << 20, 0.0};
  double atDimmest = 0.0;
  for (int step = 0; step < 12; ++step) {
    const double bearing = (double)step * 30.0;
    std::vector<uint8_t> rgba;
    if (!engine.Declare(Under(bearing)) || !engine.Pixels(rgba)) {
      Unprepared(("a picture did not come back: " + engine.Error()).c_str());
      return Report();
    }
    const Read said = Judged(rgba);
    if (said.Peak > brightest) {
      brightest = said.Peak;
      atBearing = bearing;
      best = said;
    }
    if (said.Peak < dimmest.Peak) {
      dimmest = said;
      atDimmest = bearing;
    }
  }

  std::printf("  the brightest bearing %5.0f deg   peak %3d   mean %6.2f\n", atBearing,
              best.Peak, best.Mean);
  std::printf("  the dimmest   bearing %5.0f deg   peak %3d   mean %6.2f\n", atDimmest,
              dimmest.Peak, dimmest.Mean);

  CHECK(dimmest.Peak > 0,
        "the sky is lit at every bearing, so what the check below measures is a DISC and not the "
        "difference between a lit sky and an unlit one");

  CHECK(best.Peak >= 250,
        "**THE SUN IS THE SKY'S OWN RADIANCE WHERE THE SUN IS**: a source of angular radius "
        "4.6542e-3 rad subtending pi*t^2 steradian and delivering 40 000 lux carries a radiance "
        "of 5.9e8, six thousand times a clear zenith. A frame with it in view has a pixel at the "
        "top of its range, and a sky that integrates in-scattering while omitting the disc has "
        "left out the brightest thing in it");

  CHECK(best.Peak > dimmest.Peak,
        "and the peak follows the sun's BEARING: the same medium, the same illuminance and the "
        "same exposure at every step, so what moves between them is where the sun is and nothing "
        "else");

  Covers("the door: the sun is the sky's own radiance in its own direction -- a disc of "
         "E/(pi t^2) attenuated by the transmittance the in-scattering already samples, not a "
         "stage of its own");
  return Report();
}
