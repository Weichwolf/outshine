#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

#include <Outshine.h>
#include <Scenario.h>

#include "Check.h"

namespace {

// TWO LAWS, both read off the pixels the door hands back.
//
// LAW ONE -- A PRESENTED FRAME IS A PICTURE, NOT A CUT-OUT.
//
// Alpha in this renderer is COVERAGE, and coverage exists so that an intermediate target can be
// composited under something drawn later: the subject pass leaves alpha 0 where it drew nothing
// so the sky beneath shows through. The PRESENTED surface has nothing left to composite against,
// so every one of its pixels owes an alpha of 255. A frame that carries alpha 0 anywhere is a
// cut-out, and a cut-out is not a picture: every image viewer composites it over its own white
// page, so a wholly BLACK render reads back as a lit one. That failure is not hypothetical --
// it is how a black driver picture was read as correct for a whole session.
//
// LAW TWO -- A DECLARED SPHERE WITH AIR CARRIES A SKY, AND THE SKY FOLLOWS THE SUN.
//
// The sky is single-scattered sunlight. Along a view ray the radiance is
//
//   L = integral over s of  beta(h(s)) * T(eye, s) * T(s, sun) * phase  ds
//
// and the factor that decides this case is T(s, sun), the transmittance from the sample point
// to the sun. With the sun BELOW the horizon the ray from every sample point toward the sun is
// blocked by the planet itself, so T(s, sun) = 0 at every s and the whole integral is 0. The
// sky is black, exactly, and not merely dim. With the sun above the horizon T(s, sun) > 0 over
// the near part of the ray and the integral is positive. So:
//
//   sun above the horizon  ->  sky bright
//   sun below the horizon  ->  sky black
//
// This is a law of the medium, not of our design: it holds for any single-scattering atmosphere
// over an opaque sphere, and it would hold if every line of the renderer were rewritten.
//
// THE CONTROL. A sky can only be credited with the difference if nothing else in the frame
// moves. So the same two elevations are rendered with NO sphere declared. Nothing is declared
// that could respond to the sun, and the two frames must be IDENTICAL. If they differ, the
// difference in the first pair belongs to something other than the sky and this case proves
// nothing. If they are identical AND both black, the sphere is what filled the picture.
constexpr int kFramePx = 64;

struct Read {
  double MeanMax = 0.0;
  int Peak = 0;
  size_t Clear = 0;
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
    if (rgba[at * 4 + 3] != 255) { said.Clear += 1; }
  }
  said.MeanMax = sum / (double)pixels;
  return said;
}

[[nodiscard]] outshine::Scenario Stood(bool withASphere, double elevationDeg) {
  outshine::Scenario made;
  made.Render.Declared = true;
  made.Render.Frame = outshine::Extent{kFramePx, kFramePx};
  made.Lit.Declared = true;
  made.Lit.Key.Lux = 40000.0;
  made.Lit.Key.ElevationDeg = elevationDeg;
  made.Lit.Key.BearingDeg = 150.0;
  if (withASphere) {
    made.Ground.Declared = true;
    made.Ground.Origin.LatitudeDeg = 48.1372;
    made.Ground.Origin.LongitudeDeg = 11.5756;
  }
  return made;
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
  engine.setRoots(outshine::Roots{".", "src/assets", "/tmp/outshine-door-cache", true});
  if (!engine.drawsInto(outshine::Extent{kFramePx, kFramePx})) {
    Unprepared("the device stood no canvas");
    return Report();
  }

  const auto shot = [&](bool sphere, double elevationDeg, std::vector<uint8_t> &into) {
    const outshine::Scenario stands = Stood(sphere, elevationDeg);
    if (!engine.declare(stands)) { return false; }
    return engine.renderer().readPixels(into).has_value();
  };

  std::vector<uint8_t> dayLit, nightly, bareDay, bareNight;
  if (!shot(true, 42.0, dayLit) || !shot(true, -80.0, nightly) ||
      !shot(false, 42.0, bareDay) || !shot(false, -80.0, bareNight)) {
    Unprepared(("a picture did not come back: " + engine.error()).c_str());
    return Report();
  }

  const Read day = Judged(dayLit), night = Judged(nightly);
  const Read plainDay = Judged(bareDay), plainNight = Judged(bareNight);
  std::printf("  sphere, sun +42 deg  mean max(RGB) %6.2f  peak %3d  pixels not opaque %zu\n",
              day.MeanMax, day.Peak, day.Clear);
  std::printf("  sphere, sun -80 deg  mean max(RGB) %6.2f  peak %3d  pixels not opaque %zu\n",
              night.MeanMax, night.Peak, night.Clear);
  std::printf("  no sphere,     +42   mean max(RGB) %6.2f  peak %3d  pixels not opaque %zu\n",
              plainDay.MeanMax, plainDay.Peak, plainDay.Clear);
  std::printf("  no sphere,     -80   mean max(RGB) %6.2f  peak %3d  pixels not opaque %zu\n",
              plainNight.MeanMax, plainNight.Peak, plainNight.Clear);

  CHECK(day.Clear == 0 && night.Clear == 0 && plainDay.Clear == 0 && plainNight.Clear == 0,
        "**A PRESENTED FRAME IS A PICTURE, NOT A CUT-OUT**: alpha is the coverage an "
        "INTERMEDIATE target carries so that a later pass can composite beneath it, and the "
        "presented surface has nothing left to composite against, so every pixel of it owes "
        "alpha 255. A frame that leaves alpha 0 where it drew nothing hands the viewer a "
        "silhouette, and every viewer composites that over its own white page -- which is how a "
        "wholly black picture reads back as a lit one");

  CHECK(day.MeanMax > 8.0,
        "**A DECLARED SPHERE WITH AIR CARRIES A SKY**: the medium is declared by its air "
        "density, not by a switch, and with the sun 42 degrees up the single-scattering "
        "integral along every view ray is positive, so the frame is not black");

  CHECK(night.MeanMax < 1.0,
        "and the sky FOLLOWS THE SUN: 80 degrees below the horizon the ray from every sample "
        "point toward the sun is blocked by the sphere itself, so T(s, sun) = 0 at every s and "
        "the scattering integral is exactly zero -- a law of the medium, not of this renderer");

  CHECK(bareDay == bareNight,
        "and the CONTROL holds: with no sphere declared the same two sun elevations make the "
        "byte-identical frame, so nothing but the sky moved between the first two pictures. Were "
        "these to differ, the brightness above would belong to something else and this case "
        "would prove nothing");

  CHECK(plainDay.MeanMax < 1.0,
        "and the control is a control: with no sphere declared the frame is BLACK, so the sky is "
        "what filled the picture and not a background painted behind it");

  return Report();
}
