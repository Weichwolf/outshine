#include <cstdio>
#include <cstdlib>
#include <span>
#include <string>
#include <vector>

#include <Event.h>
#include <Geometry.h>
#include <Outshine.h>
#include <Scenario.h>

#include "Check.h"

namespace {

// THE SCENARIO DECLARES THE SKY AND EVERYTHING ELSE ARRIVES WITH THE CONTENT. A headlamp belongs to
// the car and a street lamp to the street, and both travel with the thing they are attached to --
// which here means the one geometry value, whether a file wrote it or a generator did.
// `KHR_lights_punctual` is ratified and the reader has honoured it all along, so a light in a FILE
// has always worked; what did not exist was the same light arriving through the door.
//
// THE ORACLE IS A DIFFERENCE AND NOT AN ABSOLUTE, which the first attempt got wrong. The key
// stands at -80 degrees, below the horizon, where `ScoreWhatALitSurfaceReads` measures 0.000 on a
// lit surface -- but the FRAME still reads 101 of 255, because the scenario's environment term
// lights it and the sky is drawn. Asserting that a sunless frame is dark asserted something the
// tree does not claim.
//
// What is clean is the DIFFERENCE: two frames identical in every declared term except the lamp, so
// whatever separates them is the lamp's. That is the oracle a lamp deserves -- a light is a thing
// that ADDS, and adding is measured by subtracting.
//
// THIS CASE WAS WRITTEN ONCE BEFORE AND WITHDRAWN THE SAME HOUR. It stood on a door that published
// `std::span<const float> PositionsM` as its storage -- a value that froze the vertex layout into
// the ABI and handed out views into the producer's own vectors. Building a capability on a door
// about to be replaced is the double work the refactor forbids, so the lamp went back with the
// door. The door is a builder now, `include/PunctualLight.h` is a door type, and the lamp can
// stand.
constexpr int kFramePx = 72;

constexpr float kWall[18] = {-2.0f, -2.0f, 0.0f, 2.0f, -2.0f, 0.0f, 2.0f, 2.0f, 0.0f,
                             -2.0f, -2.0f, 0.0f, 2.0f, 2.0f,  0.0f, -2.0f, 2.0f, 0.0f};
constexpr float kFacing[18] = {0, 0, 1, 0, 0, 1, 0, 0, 1, 0, 0, 1, 0, 0, 1, 0, 0, 1};
constexpr uint32_t kRun[6] = {0, 1, 2, 3, 4, 5};

[[nodiscard]] double Brightest(const std::vector<uint8_t> &rgba) {
  const size_t pixels = rgba.size() / 4;
  double most = 0.0;
  for (size_t at = 0; at < pixels; ++at) {
    for (int channel = 0; channel < 3; ++channel) {
      const double one = rgba[at * 4 + (size_t)channel];
      if (one > most) { most = one; }
    }
  }
  return most;
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

  outshine::Scenario stands;
  stands.Render.Declared = true;
  stands.Render.Frame = outshine::Extent{kFramePx, kFramePx};
  stands.Render.Fill = 0.9;
  stands.Lit.Declared = true;
  stands.Lit.Key.Lux = 40000.0;
  stands.Lit.Key.ElevationDeg = -80.0;
  if (!engine.Declare(stands)) {
    Unprepared(("the scenario would not stand: " + engine.Error()).c_str());
    return Report();
  }

  const auto drawn = [&](bool withLamp, std::vector<uint8_t> &rgba) {
    outshine::Geometry geometry;
    outshine::Material chalk;
    chalk.BaseColour[0] = 0.9f;
    chalk.BaseColour[1] = 0.9f;
    chalk.BaseColour[2] = 0.9f;
    chalk.Roughness = 0.9f;
    const int named = geometry.Surface("chalk", chalk);
    const int part = geometry.Part("wall", named);
    if (withLamp) {
      outshine::PunctualLight lamp;
      lamp.Kind = outshine::LightKind::Point;
      lamp.Intensity = 8000.0f;
      lamp.RangeM = 40.0f;
      const double placed[16] = {1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0.0, 0.0, 3.0, 1};
      (void)geometry.Lamp("lamp", lamp, placed);
    }
    return geometry.Positions(part, std::span<const float>(kWall, 18)) &&
           geometry.Normals(part, std::span<const float>(kFacing, 18)) &&
           geometry.Triangles(part, std::span<const uint32_t>(kRun, 6)) &&
           engine.Stands(geometry) && engine.RenderTo(outshine::Extent{}) && engine.Pixels(rgba);
  };

  std::vector<uint8_t> dark;
  if (!drawn(false, dark)) {
    Unprepared(("the wall under no sky would not draw: " + engine.Error()).c_str());
    return Report();
  }
  std::vector<uint8_t> lamplit;
  if (!drawn(true, lamplit)) {
    Unprepared(("the wall under a handed lamp would not draw: " + engine.Error()).c_str());
    return Report();
  }

  std::printf("KEY BELOW THE HORIZON    brightest %6.2f of 255\n", Brightest(dark));
  std::printf("AND ONE LAMP HANDED IN   brightest %6.2f of 255\n", Brightest(lamplit));

  CHECK(Brightest(dark) > 0.0 && Brightest(dark) < 250.0,
        "the sunless frame is neither black nor blown out, so the difference below is read between "
        "two pictures that both exist -- the environment term and the sky light it, and a case "
        "that assumed darkness would be asserting something this tree does not claim");
  CHECK(Brightest(lamplit) > Brightest(dark) + 2.0,
        "**A LAMP HANDED IN THROUGH THE DOOR LIGHTS THE PICTURE**: `KHR_lights_punctual` is "
        "ratified and the reader has honoured it all along, so a light in a FILE always worked. "
        "The value a client and a generator hand in carried none, so a lamp's MESH could arrive "
        "and its LIGHT could not -- a capability present on one side of a value and absent on the "
        "other, which is the shape this tree keeps producing");

  Covers("the door: a scenario declares the sky and everything else arrives with the content, so "
         "a punctual light handed in through the one geometry value lights the picture exactly as "
         "one read from a glTF file does");
  return Report();
}
