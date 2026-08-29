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

// THE ORACLE IS WHAT A MATERIAL IS FOR: it states what a surface looks like, so a surface that
// states red and renders grey did not state anything. Hand in the same triangle twice under two
// base colours and the picture must differ, in the channel the declaration named and not another.
//
// WHAT THIS CLOSES is one row of a table where the reader ran far ahead of the door. Nineteen glTF
// extensions reach the picture; the value a client or a generator hands in carried seven vertex
// streams and a material INDEX -- an int into a table that only a FILE could supply, because
// materials reached the renderer from `Gltf::Document` (`src/engine/Live.cpp:57` walks
// `file.Materials()`) and never from the subject. So a generator could state that its part used
// material 2 and had no way to say what material 2 was.
//
// The repair is not a second path for handed materials. `outshine::Material` already carried the
// whole PBR row and all nine `KHR_materials_*` -- it was simply on the wrong side of the door, in
// `src/content/shade/`. It moved to `include/`, `Geometry` gained `Surface(named, material)`, and
// `Gltf::Subject` now HOLDS the surfaces it was assembled with. Both producers fill the same list:
// the reader copies the document's materials into it, the builder copies the client's. The surface
// table reads that one list and no longer asks a file.
//
// Textures are not here yet and this case does not pretend otherwise: an image needs the file's
// buffers, so `Geometry` cannot carry one until it carries images. What a handed surface states
// today is the row -- colour, metalness, roughness, transmission, emission and the rest.
constexpr int kFramePx = 72;

constexpr float kPositions[18] = {-2.0f, -2.0f, 0.0f, 2.0f, -2.0f, 0.0f, 2.0f, 2.0f, 0.0f,
                                  -2.0f, -2.0f, 0.0f, 2.0f, 2.0f,  0.0f, -2.0f, 2.0f, 0.0f};
constexpr float kNormals[18] = {0, 0, 1, 0, 0, 1, 0, 0, 1, 0, 0, 1, 0, 0, 1, 0, 0, 1};
constexpr uint32_t kIndices[6] = {0, 1, 2, 3, 4, 5};

struct Lit {
  double Red, Green, Blue;
};

[[nodiscard]] Lit Mean(const std::vector<uint8_t> &rgba) {
  const size_t pixels = rgba.size() / 4;
  if (pixels == 0) { return Lit{0, 0, 0}; }
  double r = 0, g = 0, b = 0;
  for (size_t at = 0; at < pixels; ++at) {
    r += rgba[at * 4];
    g += rgba[at * 4 + 1];
    b += rgba[at * 4 + 2];
  }
  return Lit{r / (double)pixels, g / (double)pixels, b / (double)pixels};
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

  outshine::Scenario stands;
  stands.Render.Declared = true;
  stands.Render.Frame = outshine::Extent{kFramePx, kFramePx};
  stands.Render.Fill = 0.6;
  stands.Lit.Declared = true;
  stands.Lit.Key.Lux = 40000.0;
  stands.Lit.Key.ElevationDeg = 42.0;
  stands.Lit.Key.BearingDeg = 0.0;
  if (!engine.declare(stands)) {
    Unprepared(("the scenario would not stand: " + engine.error()).c_str());
    return Report();
  }

  const auto drawn = [&](const float colour[4], std::vector<uint8_t> &rgba) {
    outshine::Geometry geometry;
    outshine::Material surface;
    for (int at = 0; at < 4; ++at) { surface.BaseColour[at] = colour[at]; }
    surface.Metalness = 0.0f;
    surface.Roughness = 0.9f;
    const outshine::MaterialInstance named = geometry.Surface("stated", surface);
    const int part = geometry.Part("face", named);
    return geometry.Positions(part, std::span<const float>(kPositions, 18)) &&
           geometry.Normals(part, std::span<const float>(kNormals, 18)) &&
           geometry.Triangles(part, std::span<const uint32_t>(kIndices, 6)) &&
           engine.setGeometry(geometry) && engine.renderer().render(outshine::Extent{}) && engine.renderer().readPixels(rgba);
  };

  constexpr float kRed[4] = {0.80f, 0.05f, 0.05f, 1.0f};
  constexpr float kBlue[4] = {0.05f, 0.05f, 0.80f, 1.0f};

  std::vector<uint8_t> red, blue;
  if (!drawn(kRed, red)) {
    Unprepared(("the red arm did not stand: " + engine.error()).c_str());
    return Report();
  }
  if (!drawn(kBlue, blue)) {
    Unprepared(("the blue arm did not stand: " + engine.error()).c_str());
    return Report();
  }

  const Lit ofRed = Mean(red);
  const Lit ofBlue = Mean(blue);
  std::printf("STATING RED   mean r %6.2f  g %6.2f  b %6.2f\n", ofRed.Red, ofRed.Green, ofRed.Blue);
  std::printf("STATING BLUE  mean r %6.2f  g %6.2f  b %6.2f\n", ofBlue.Red, ofBlue.Green,
              ofBlue.Blue);

  CHECK(ofRed.Red > 1.0 || ofBlue.Blue > 1.0,
        "something was drawn at all, so the comparison below has two pictures and not two black "
        "frames agreeing with each other");
  CHECK(ofRed.Red > ofRed.Blue + 1.0,
        "**A SURFACE HANDED IN THROUGH THE DOOR STATES ITS OWN COLOUR**: a material says what a "
        "surface looks like, and a part that states red must not render grey. Materials reached "
        "the renderer from the glTF DOCUMENT and never from the subject, so a client or a "
        "generator could name material 2 and had no way to say what material 2 was");
  CHECK(ofBlue.Blue > ofBlue.Red + 1.0,
        "and it is the STATED channel that answers, not merely a difference: swapping red for "
        "blue moves blue above red, which a picture that ignored the declaration and picked its "
        "own colour could not do");

  // AND A PART THAT STATES NO NORMALS IS STILL LIT. glTF lets a primitive omit NORMAL, and the
  // reader has always answered that by generating FLAT normals -- splitting shared vertices so each
  // triangle carries its own facing. The packer a handed value goes through did not, so geometry
  // from a client or a generator arrived with no facing at all and the light had nothing to fall
  // on. One packer, one answer: `Assemble` runs the same generation the reader does.
  const auto bare = [&](std::vector<uint8_t> &rgba) {
    outshine::Geometry geometry;
    outshine::Material surface;
    for (int at = 0; at < 4; ++at) { surface.BaseColour[at] = kRed[at]; }
    surface.Roughness = 0.9f;
    const outshine::MaterialInstance named = geometry.Surface("stated", surface);
    const int part = geometry.Part("face", named);
    return geometry.Positions(part, std::span<const float>(kPositions, 18)) &&
           geometry.Triangles(part, std::span<const uint32_t>(kIndices, 6)) &&
           engine.setGeometry(geometry) && engine.renderer().render(outshine::Extent{}) && engine.renderer().readPixels(rgba);
  };
  std::vector<uint8_t> unfaced;
  const bool stoodBare = bare(unfaced);
  const std::string whyBare = engine.error();
  const Lit ofBare = stoodBare ? Mean(unfaced) : Lit{0, 0, 0};
  std::printf("STATING NO NORMALS  %s mean r %6.2f  g %6.2f  b %6.2f\n",
              stoodBare ? "        " : "REFUSED,", ofBare.Red, ofBare.Green, ofBare.Blue);
  if (!stoodBare) { std::printf("  BECAUSE %s\n", whyBare.c_str()); }

  CHECK(stoodBare && ofBare.Red > 1.0,
        "**A HANDED PART THAT STATES NO NORMALS IS STILL LIT**: glTF lets a primitive omit "
        "NORMAL and the reader answers with FLAT normals, splitting shared vertices so each "
        "triangle carries its own facing. The packer a handed value went through did not, so a "
        "client's or a generator's geometry arrived with no facing and the light had nothing to "
        "fall on -- one packer now, and one answer");

  Covers("the door: a client or a generator hands in the material its part wears, and the picture "
         "shows that material -- `outshine::Material` is a door type and the subject holds the "
         "surfaces it was assembled with, whichever producer filled them");
  return Report();
}
