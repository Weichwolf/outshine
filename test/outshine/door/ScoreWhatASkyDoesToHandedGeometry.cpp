#include <cmath>
#include <cstdio>
#include <span>
#include <string>
#include <vector>

#include <Geometry.h>
#include <Outshine.h>
#include <Scenario.h>

#include "Check.h"

// WHAT A SKY DOES TO HANDED GEOMETRY, asked because a cause was written down without measuring it.
//
// A lighting case for handed geometry failed three times and I recorded the reason as: declaring
// `Ground.Declared` to obtain a sky also stands a WORLD, whose bounds then drive the framing, so
// the quad becomes a few pixels and every mean is background. That is a hypothesis. It was written
// into board:2020 as a finding, and this case is what it should have had first.
//
//   DRAWN         a handed quad with no sky declared reaches the frame
//   STILL DRAWN   the same quad with a sky declared reaches it too, and covers a comparable share
//
// If the second holds, the recorded cause is wrong and the three failures have another one. If it
// fails, a client cannot light handed geometry with this engine's sky at all, which is a door
// finding rather than a lighting one.
//
// WHAT THIS DOES NOT COVER: it says nothing about whether the LIGHTING is right in either arm, and
// nothing about placing handed geometry on the globe -- `setGeometry` takes no placement, so the
// question of where a quad stands relative to a georeferenced world is not asked here.

namespace {

constexpr int kFramePx = 96;

constexpr float kFace[18] = {-2.0f,
                             -2.0f,
                             0.0f,
                             2.0f,
                             -2.0f,
                             0.0f,
                             2.0f,
                             2.0f,
                             0.0f,
                             -2.0f,
                             -2.0f,
                             0.0f,
                             2.0f,
                             2.0f,
                             0.0f,
                             -2.0f,
                             2.0f,
                             0.0f};
constexpr float kFacing[18] = {0, 0, 1, 0, 0, 1, 0, 0, 1, 0, 0, 1, 0, 0, 1, 0, 0, 1};
constexpr uint32_t kRun[6] = {0, 1, 2, 3, 4, 5};

[[nodiscard]] double Covered(const std::vector<uint8_t> &rgba) {
  const size_t pixels = rgba.size() / 4;
  if (pixels == 0) { return 0.0; }
  size_t red = 0;
  for (size_t at = 0; at < pixels; ++at) {
    if (rgba[at * 4] > rgba[at * 4 + 2] + 20) { ++red; }
  }
  return (double)red / (double)pixels;
}

} // namespace

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

  // The quad is RED so its pixels can be told from any sky by hue rather than by a coordinate. A
  // magic pixel would be a number with no source; a red-over-blue test is one the material states.
  const auto drawn = [&](bool withSky, std::vector<uint8_t> &rgba) {
    outshine::Scenario stands;
    stands.Ground.Declared = withSky;
    stands.Render.Declared = true;
    stands.Render.Frame = outshine::Extent{kFramePx, kFramePx};
    stands.Render.Fill = 0.8;
    stands.Lit.Declared = true;
    stands.Lit.Key.Lux = 40000.0;
    stands.Lit.Key.ElevationDeg = 45.0;
    stands.Lit.Key.BearingDeg = 0.0;
    if (!engine.declare(stands)) { return false; }
    outshine::Geometry geometry;
    outshine::Material scarlet;
    scarlet.BaseColour[0] = 0.85f;
    scarlet.BaseColour[1] = 0.05f;
    scarlet.BaseColour[2] = 0.05f;
    scarlet.Roughness = 0.9f;
    const outshine::MaterialInstance named = geometry.addSurface("scarlet", scarlet);
    const int part = geometry.addPart("face", named);
    return geometry.setPositions(part, std::span<const float>(kFace, 18)) &&
           geometry.setNormals(part, std::span<const float>(kFacing, 18)) &&
           geometry.setTriangles(part, std::span<const uint32_t>(kRun, 6)) &&
           engine.setGeometry(geometry) && engine.renderer().render(outshine::Extent{}) &&
           engine.renderer().readPixels(rgba);
  };

  std::vector<uint8_t> bare, skied;
  if (!drawn(false, bare)) {
    Unprepared(("the sky-less arm did not stand: " + engine.error()).c_str());
    return Report();
  }
  if (!drawn(true, skied)) {
    Unprepared(("the skied arm did not stand: " + engine.error()).c_str());
    return Report();
  }

  const double withoutSky = Covered(bare);
  const double withSky = Covered(skied);
  std::printf("QUAD COVERAGE   no sky %6.3f   sky declared %6.3f\n", withoutSky, withSky);

  CHECK(withoutSky > 0.05,
        "**THE CONTROL: HANDED GEOMETRY REACHES THE FRAME AT ALL**. Without this the comparison "
        "below is between two empty pictures agreeing with each other, which is the shape of a "
        "proof that proves nothing");

  CHECK(withSky > 0.5 * withoutSky,
        "**A DECLARED SKY DOES NOT SHRINK HANDED GEOMETRY OUT OF THE FRAME**: board:2020 recorded, "
        "without measuring it, that declaring `Ground` to obtain a sky also stands a world whose "
        "bounds drive the framing. If that were so the quad would collapse to a few pixels here. "
        "Whichever way this reads, the item's recorded cause is settled by it rather than by "
        "argument");

  Covers("board:2020 -- whether a declared sky costs handed geometry its framing");
  return Report();
}
