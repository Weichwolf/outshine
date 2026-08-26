#include <cstdio>
#include <cstdlib>
#include <span>
#include <string>
#include <vector>

#include <Event.h>
#include <Generate.h>
#include <Outshine.h>
#include <Scenario.h>

#include "Check.h"

namespace {

// THE ORACLE IS WHAT A DECLARATION IS. A scenario states what shall stand; the engine either
// stands it or refuses and says why. A line that is parsed, merged across layers, COUNTED, and
// then dropped is neither -- it reads as accepted and does nothing, which is the worst of the
// three outcomes because nothing anywhere says so.
//
// That was the state of `<generators>`. `include/Scenario.h` has carried
//
//     struct Generator { std::string Kind; std::vector<Setting> Parameters; };
//
// all along; `ScenarioRead` parses it, `ScenarioLayer` merges it across layers, and
// `Engine::Declare` counts it into the carried notes. Nothing resolved `Kind` to anything that
// runs. A client could declare a forest and get silence.
//
// TWO HALVES, AND BOTH ARE HERE. A kind nobody offers is REFUSED at declaration, by name -- that
// is the half a runtime check cannot do later and an assembly-time refusal can. And a kind that IS
// offered runs, and what it makes stands in the picture.
//
// THE SHAPE IS A NAME, NOT A POINTER, and that is not a style preference. A scenario is a value
// written to XML and read back; a pointer does not survive the round trip and a name does. Unreal
// references an asset by object path through the asset registry and RAGE by name or hash through
// streaming -- neither puts a raw pointer in a map. So a client REGISTERS its generator under a
// kind and the declaration names that kind, which is exactly what `Generator{Kind, Parameters}`
// was already shaped for.
constexpr int kFramePx = 72;

constexpr float kPositions[18] = {-2.0f, -2.0f, 0.0f, 2.0f, -2.0f, 0.0f, 2.0f, 2.0f, 0.0f,
                                  -2.0f, -2.0f, 0.0f, 2.0f, 2.0f,  0.0f, -2.0f, 2.0f, 0.0f};
constexpr float kNormals[18] = {0, 0, 1, 0, 0, 1, 0, 0, 1, 0, 0, 1, 0, 0, 1, 0, 0, 1};
constexpr uint32_t kIndices[6] = {0, 1, 2, 3, 4, 5};

class Slab final : public outshine::Generates {
public:
  [[nodiscard]] std::string_view Kind() const override { return "test-slab"; }

  [[nodiscard]] bool Make(const outshine::Ask &ask, outshine::Geometry &into) const override {
    Asked = ask;
    outshine::Material surface;
    surface.BaseColour[0] = 0.05f;
    surface.BaseColour[1] = 0.75f;
    surface.BaseColour[2] = 0.05f;
    surface.Roughness = 0.9f;
    const int named = into.Surface("slab", surface);
    const int part = into.Part("slab", named);
    return into.Positions(part, std::span<const float>(kPositions, 18)) &&
           into.Normals(part, std::span<const float>(kNormals, 18)) &&
           into.Triangles(part, std::span<const uint32_t>(kIndices, 6));
  }

  mutable outshine::Ask Asked;
};

[[nodiscard]] double Green(const std::vector<uint8_t> &rgba) {
  const size_t pixels = rgba.size() / 4;
  if (pixels == 0) { return 0.0; }
  double sum = 0.0;
  for (size_t at = 0; at < pixels; ++at) { sum += rgba[at * 4 + 1]; }
  return sum / (double)pixels;
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
  stands.Render.Fill = 0.6;
  stands.Lit.Declared = true;
  stands.Lit.Key.Lux = 40000.0;
  stands.Lit.Key.ElevationDeg = 42.0;
  stands.Lit.Key.BearingDeg = 0.0;

  outshine::Generator wanted;
  wanted.Kind = "test-slab";
  stands.Generators.push_back(wanted);

  const bool refused = !engine.Declare(stands);
  const std::string why = engine.Error();
  std::printf("DECLARED, NOBODY OFFERS IT   %s\n", refused ? why.c_str() : "STOOD ANYWAY");

  CHECK(refused,
        "**A GENERATOR NOBODY OFFERS IS REFUSED AT DECLARATION**: a scenario states what shall "
        "stand, and a line that is parsed, merged across layers and COUNTED but never resolved "
        "reads as accepted while doing nothing -- the worst of the three outcomes, because "
        "nothing anywhere says so");
  CHECK(why.find("test-slab") != std::string::npos,
        "and the refusal NAMES the kind it could not find, because a refusal that does not say "
        "which line is at fault sends the reader back to the whole file");

  Slab slab;
  engine.Offers(slab);
  std::vector<uint8_t> pixels;
  if (!engine.Declare(stands) || !engine.RenderTo(outshine::Extent{}) || !engine.Pixels(pixels)) {
    Unprepared(("the offered generator did not stand: " + engine.Error()).c_str());
    return Report();
  }
  const double green = Green(pixels);
  std::printf("OFFERED, THEN DECLARED       asked at east %.4f north %.4f over %.1f m, "
              "mean green %.2f\n",
              slab.Asked.EastM, slab.Asked.NorthM, slab.Asked.ExtentM, green);

  CHECK(green > 1.0,
        "**AND A GENERATOR THAT IS OFFERED RUNS, AND WHAT IT MAKES STANDS IN THE PICTURE**: the "
        "client registers under a KIND and the declaration names that kind, which is what "
        "`Generator{Kind, Parameters}` was always shaped for. A pointer in the declaration would "
        "have read the same and could not survive being written to XML and read back");

  outshine::Scenario asAsset;
  asAsset.Render = stands.Render;
  asAsset.Lit = stands.Lit;
  outshine::Asset generated;
  generated.Uri = "test-slab";
  generated.Kind = "generated";
  asAsset.Assets.push_back(generated);

  std::vector<uint8_t> byAsset;
  const bool stoodAsAsset = engine.Declare(asAsset) && engine.RenderTo(outshine::Extent{}) &&
                            engine.Pixels(byAsset);
  const double asAssetGreen = stoodAsAsset ? Green(byAsset) : -1.0;

  outshine::Scenario unknown = asAsset;
  unknown.Assets.front().Uri = "no-such-maker";
  const bool refusedAsset = !engine.Declare(unknown);
  const std::string whyAsset = engine.Error();

  std::printf("AS A GENERATED ASSET         mean green %.2f\n", asAssetGreen);
  std::printf("NAMING A MAKER NOBODY OFFERS %s\n",
              refusedAsset ? whyAsset.c_str() : "STOOD ANYWAY");

  CHECK(asAssetGreen > 1.0,
        "**AND A SCENARIO'S ASSET MAY NAME A GENERATOR INSTEAD OF A FILE**: `<asset "
        "kind=\"generated\" uri=\"test-slab\"/>` stands what the maker makes, so a scenario "
        "USES the geometry a client builds without a pointer in the declaration -- a pointer "
        "cannot be written to XML and read back, and a name can");
  CHECK(refusedAsset && whyAsset.find("no-such-maker") != std::string::npos,
        "and an asset naming a maker nobody offers is refused by that name, the same way a "
        "declared generator is -- one resolution, two places to name it from");

  Covers("the door: a scenario's declared generator resolves against what the client offers -- an "
         "unknown kind is refused by name at declaration, an offered one runs and reaches the "
         "picture, and an ASSET may name a generator instead of a file");
  return Report();
}
