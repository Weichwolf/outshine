#include <cmath>
#include <cstdio>
#include <span>
#include <string>
#include <vector>

#include <Geometry.h>
#include <Outshine.h>
#include <Scenario.h>

#include "Check.h"

// WHICH WAY THE AMBIENT LEANS -- the one thing a CONSTANT ambient cannot answer.
//
// Unreal captures a `SkyLight` including the ground; RAGE blends a sky and a ground ambient on the
// up axis; Filament projects an environment into spherical harmonics. All three make indirect light
// depend on the surface NORMAL. This tree applied one radiance to every normal alike, so a face
// turned at the ground was lit by the sky exactly as brightly as one turned at the sky, and the
// lower hemisphere -- the bounce off whatever the frame stands over -- contributed nothing at all.
//
// THE CLAIM IS A HUE, NOT A BRIGHTNESS, and that is what makes it decidable here. A face turned
// DOWN under a sun 45 degrees up has `n.l < 0` and takes no direct light in either model, so its
// colour is the indirect term and nothing else. Under the old constant that term was the sky and
// the face would read BLUE. Under a hemisphere it is the ground bounce, and the engine's default
// ground albedo is (0.10, 0.13, 0.07) -- green-dominant, and green-dominant for a reason a client
// can read rather than a number this case picked.
//
//   LIT APART    the up-turned arm is brighter than the down-turned one, so the two genuinely
//                differ in direct light and the down arm's colour really is ambient alone
//   LEANS GREEN  the down-turned arm is green-dominant. A constant sky ambient makes it blue
//   NO CAST      the sunlit arm comes back NEUTRAL. Without this, "green" could just mean the whole
//                picture carries a green cast, and the claim above would prove nothing
//
// Three earlier drafts of this case failed and the reason is worth keeping: each measured the VIEW
// rather than the normal. One let the framing stand the eye BELOW a horizontal quad, so a
// double-sided face turned its normal at the viewer and the arm labelled "up" was shaded as if it
// pointed down. Two put the sun on the horizon to zero `n.l` for both arms, which does isolate the
// ambient and also makes the frame too dark to read -- every mean was under 1.5 of 255, which is
// noise. `outshine/door/ScoreWhatASkyDoesToHandedGeometry` settled the cause I had guessed at
// instead: a declared sky does NOT cost handed geometry its framing, it covers MORE of the frame.
//
// WHAT THIS CASE DOES NOT COVER, on its own page: nothing here says the bounce's colour is right for
// any real place -- with no world declared the albedo is the engine's default, not a land class. It
// reads the frame's CENTRE as the quad, which a centred framing makes true and which would stop
// being true if the framing changed. It does NOT read the corner as sky: with no georeference
// declared no sky dome is drawn and the corner is quad as well, measured at (52, 59, 30) -- a first
// draft asserted a blue corner and went red on a sky that was never there. And it says nothing
// about magnitude: no oracle in this tree states how bright open shade should be.

namespace {

constexpr int kFramePx = 96;

constexpr float kFace[18] = {-2.0f, -2.0f, 0.0f, 2.0f, -2.0f, 0.0f, 2.0f, 2.0f, 0.0f,
                             -2.0f, -2.0f, 0.0f, 2.0f, 2.0f,  0.0f, -2.0f, 2.0f, 0.0f};
constexpr uint32_t kRun[6] = {0, 1, 2, 3, 4, 5};

struct Lit {
  double Red, Green, Blue;

  [[nodiscard]] double Sum(void) const { return Red + Green + Blue; }
};

[[nodiscard]] Lit Middle(const std::vector<uint8_t> &rgba, int side, double share) {
  const int from = (int)((0.5 - share * 0.5) * (double)side);
  const int to = (int)((0.5 + share * 0.5) * (double)side);
  double r = 0, g = 0, b = 0;
  size_t counted = 0;
  for (int row = from; row < to; ++row) {
    for (int column = from; column < to; ++column) {
      const size_t at = ((size_t)row * (size_t)side + (size_t)column) * 4u;
      if (at + 2 >= rgba.size()) { continue; }
      r += rgba[at];
      g += rgba[at + 1];
      b += rgba[at + 2];
      ++counted;
    }
  }
  return counted > 0 ? Lit{r / (double)counted, g / (double)counted, b / (double)counted}
                     : Lit{0, 0, 0};
}

[[nodiscard]] Lit Corner(const std::vector<uint8_t> &rgba, int side) {
  double r = 0, g = 0, b = 0;
  size_t counted = 0;
  for (int row = 0; row < side / 12; ++row) {
    for (int column = 0; column < side / 12; ++column) {
      const size_t at = ((size_t)row * (size_t)side + (size_t)column) * 4u;
      if (at + 2 >= rgba.size()) { continue; }
      r += rgba[at];
      g += rgba[at + 1];
      b += rgba[at + 2];
      ++counted;
    }
  }
  return counted > 0 ? Lit{r / (double)counted, g / (double)counted, b / (double)counted}
                     : Lit{0, 0, 0};
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

  // The sun stands 45 degrees up due north, so l = (0, 0.707, 0.707). The two normals lean 0.31
  // toward the eye and 0.95 up or down, which gives n.l = +0.89 for the up arm and -0.45 for the
  // down arm: one is lit and the other cannot be, in this model or the one before it. Both lean the
  // SAME way in z, so neither is ever the far side of the quad and no face is turned by the viewer.
  const auto drawn = [&](float upward, std::vector<uint8_t> &rgba) {
    float facing[18];
    const float lean = 0.31f;
    const float length = std::sqrt(0.95f * 0.95f + lean * lean);
    for (int corner = 0; corner < 6; ++corner) {
      facing[corner * 3] = 0.0f;
      facing[corner * 3 + 1] = 0.95f * upward / length;
      facing[corner * 3 + 2] = lean / length;
    }
    outshine::Scenario stands;
    stands.Ground.Declared = true;
    stands.Render.Declared = true;
    stands.Render.Frame = outshine::Extent{kFramePx, kFramePx};
    stands.Render.Fill = 0.45;
    stands.Lit.Declared = true;
    stands.Lit.Key.Lux = 40000.0;
    stands.Lit.Key.ElevationDeg = 45.0;
    stands.Lit.Key.BearingDeg = 0.0;
    if (!engine.declare(stands)) { return false; }
    outshine::Geometry geometry;
    outshine::Material plain;
    plain.BaseColour[0] = 0.70f;
    plain.BaseColour[1] = 0.70f;
    plain.BaseColour[2] = 0.70f;
    plain.Roughness = 0.9f;
    const outshine::MaterialInstance named = geometry.addSurface("plain", plain);
    const int part = geometry.addPart("face", named);
    return geometry.setPositions(part, std::span<const float>(kFace, 18)) &&
           geometry.setNormals(part, std::span<const float>(facing, 18)) &&
           geometry.setTriangles(part, std::span<const uint32_t>(kRun, 6)) &&
           engine.setGeometry(geometry) && engine.renderer().render(outshine::Extent{}) &&
           engine.renderer().readPixels(rgba);
  };

  std::vector<uint8_t> skyward, groundward;
  if (!drawn(1.0f, skyward) || !drawn(-1.0f, groundward)) {
    Unprepared(("an arm did not stand: " + engine.error()).c_str());
    return Report();
  }

  const Lit up = Middle(skyward, kFramePx, 0.25);
  const Lit down = Middle(groundward, kFramePx, 0.25);
  const Lit sky = Corner(groundward, kFramePx);
  std::printf("QUAD, NORMAL UP     r %6.2f  g %6.2f  b %6.2f\n", up.Red, up.Green, up.Blue);
  std::printf("QUAD, NORMAL DOWN   r %6.2f  g %6.2f  b %6.2f\n", down.Red, down.Green, down.Blue);
  std::printf("SKY at the corner   r %6.2f  g %6.2f  b %6.2f\n", sky.Red, sky.Green, sky.Blue);

  CHECK(up.Sum() > down.Sum() + 3.0,
        "**THE CONTROL: THE TWO ARMS DIFFER IN DIRECT LIGHT**. `n.l` is +0.89 for the up arm and "
        "-0.45 for the down one, so the up arm takes the sun and the down arm cannot. If they match "
        "here, the down arm is being lit by something the case has not accounted for and its colour "
        "below is not the indirect term alone");

  const double tintOfTheLit = (up.Green - up.Red) / up.Red;
  std::printf("the sunlit arm's green over red: %+.3f of red\n", tintOfTheLit);
  CHECK(tintOfTheLit < 0.06,
        "**THE SECOND CONTROL: THE SUNLIT ARM IS NEUTRAL**. A grey material under a white sun must "
        "come back grey. If the whole picture carried a green cast the claim below would be reading "
        "that cast rather than a hemisphere, and this is what tells the two apart. The frame's "
        "corner cannot serve here: with no georeference declared there is no sky dome drawn behind "
        "the quad, and the corner reads the quad as well -- which the case says rather than "
        "quietly sampling it anyway");

  CHECK(down.Green > down.Red && down.Green > down.Blue,
        "**THE AMBIENT LEANS, AND THE LOWER HALF IS THE GROUND**: a face taking no direct light "
        "reads its indirect term neat. Under one constant radiance that term was the SKY and this "
        "face would come back blue, like the corner above it. It comes back green because the lower "
        "hemisphere is now the ground bounce, whose default albedo (0.10, 0.13, 0.07) is "
        "green-dominant. Unreal, RAGE and Filament all make ambient depend on the normal; a "
        "constant cannot, and this is the difference a constant forbids");

  Covers("board:2020 -- indirect light depends on the normal, and the lower hemisphere exists");
  return Report();
}
