#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

#include <SDL3/SDL.h>

#include <Outshine.h>
#include <Scenario.h>

#include "Check.h"

// WHAT A SECOND DECLARED SUBJECT DOES, WHICH TODAY IS NOTHING AND SAYS NOTHING.
//
// Unreal's `FScene` holds one `FPrimitiveSceneProxy` per primitive and a level declaring two
// meshes shows two; RAGE puts every `fwEntity` on the draw list of the node it stands in. Both
// agree that the count of things declared is the count of things drawn, and neither has a path
// where a second one is dropped -- there is nowhere for it to be dropped FROM.
//
// Here the picture holds ONE subject: `Engine::State::Draws` carries
// `Ticking.Freestanding.front()` and `Live` stands over one glTF document. A declaration naming
// two assets therefore draws one of them, and nothing refuses. That is the quietest kind of
// wrong -- the scenario is accepted, the frame renders, and half of what was asked for is
// missing with no line anywhere saying so.
//
// THE ORACLE IS COUNTING AND OWES NOTHING TO OUR DESIGN: two identical subjects draw twice the
// batches of one, or the engine says why not. Written as a RATIO of two runs rather than against
// a constant, so the number cannot be tuned to whatever the tree happens to do.
//
// This is board:1574's measurement, and it is what blocks board:1957: a delta over a one-row
// placement table is a line of code rather than an architecture.

namespace {

constexpr int kFramePx = 64;

constexpr const char *kTriangleBase64 =
    "AAAAAAAAAAAAAAAAAACAPwAAAAAAAAAAAAAAAAAAgD8AAAAAAAAAAAAAAAAAAIA/AAAAAAAAAAAAAIA/AAAAAAAAAA"
    "AAAIA/";

[[nodiscard]] std::string Minimal(void) {
  return std::string(
             "{\"asset\":{\"version\":\"2.0\"},\"scene\":0,\"scenes\":[{\"nodes\":[0]}],"
             "\"nodes\":[{\"mesh\":0}],"
             "\"meshes\":[{\"primitives\":[{\"attributes\":{\"POSITION\":0,\"NORMAL\":1},"
             "\"material\":0}]}],"
             "\"materials\":[{\"pbrMetallicRoughness\":{\"baseColorFactor\":[0.8,0.8,0.8,1.0]}}],"
             "\"accessors\":[{\"bufferView\":0,\"componentType\":5126,\"count\":3,\"type\":"
             "\"VEC3\","
             "\"min\":[0,0,0],\"max\":[1,1,0]},"
             "{\"bufferView\":1,\"componentType\":5126,\"count\":3,\"type\":\"VEC3\"}],"
             "\"bufferViews\":[{\"buffer\":0,\"byteOffset\":0,\"byteLength\":36},"
             "{\"buffer\":0,\"byteOffset\":36,\"byteLength\":36}],"
             "\"buffers\":[{\"byteLength\":72,\"uri\":\"data:application/octet-stream;base64,") +
         kTriangleBase64 + "\"}]}";
}

[[nodiscard]] bool Wrote(const std::string &path, const std::string &held) {
  std::FILE *const file = std::fopen(path.c_str(), "wb");
  if (file == nullptr) { return false; }
  const bool whole = std::fwrite(held.data(), 1, held.size(), file) == held.size();
  return std::fclose(file) == 0 && whole;
}

[[nodiscard]] double Measured(const outshine::Engine &engine, const char *what) {
  for (const outshine::Measure &held : engine.measures()) {
    if (held.What == what) { return held.How; }
  }
  return -1.0;
}

[[nodiscard]] outshine::Scenario Naming(const std::vector<const char *> &assets) {
  outshine::Scenario stands;
  stands.Render.Declared = true;
  stands.Render.Frame = outshine::Extent{kFramePx, kFramePx};
  stands.Render.Fill = 0.6;
  stands.Lit.Declared = true;
  stands.Lit.Key.Lux = 40000.0;
  stands.Lit.Key.ElevationDeg = 42.0;
  stands.Lit.Key.BearingDeg = 0.0;
  for (const char *uri : assets) {
    outshine::Asset shown;
    shown.Uri = uri;
    shown.Kind = "gltf";
    stands.Assets.push_back(shown);
  }
  return stands;
}

} // namespace

int main(void) {
  using namespace outshine::Test;
  std::setvbuf(stdout, nullptr, _IONBF, 0);

  const char *nest = std::getenv("OUTSHINE_NEST");
  if (nest == nullptr || *nest == 0) {
    Unprepared("this case writes its subjects into the runner's nest and was given none");
    return Report();
  }
  const std::string under = nest;
  if (!Wrote(under + "/one.gltf", Minimal()) || !Wrote(under + "/two.gltf", Minimal())) {
    Unprepared("the subjects could not be written into the nest");
    return Report();
  }
  if (!SDL_Init(SDL_INIT_VIDEO)) {
    Unprepared("SDL did not start, so nothing can be rendered");
    return Report();
  }

  double alone = 0.0, together = 0.0;
  double pushedAlone = 0.0, pushedTogether = 0.0;
  std::string refused;
  for (int pass = 0; pass < 2; ++pass) {
    outshine::Engine engine;
    engine.setRoots(outshine::Roots{under, "src/assets", "/tmp/outshine-door-cache", true});
    if (!engine.drawsInto(outshine::Extent{kFramePx, kFramePx})) {
      Unprepared("the device stood no canvas");
      return Report();
    }
    const outshine::Scenario stands =
        pass == 0 ? Naming({"one.gltf"}) : Naming({"one.gltf", "two.gltf"});
    if (!engine.declare(stands) || !engine.advance() ||
        !engine.renderer().render(outshine::Extent{})) {
      if (pass == 1) {
        refused = engine.error();
        break;
      }
      Unprepared(engine.error().c_str());
      return Report();
    }
    (pass == 0 ? alone : together) = Measured(engine, "batches the picture draws");
    (pass == 0 ? pushedAlone : pushedTogether) =
        Measured(engine, "vertex uniform pushes the subject stages make");
  }

  if (!refused.empty()) {
    std::printf(
        "  one subject draws %.0f batch(es); TWO were REFUSED: %s\n", alone, refused.c_str());
  } else {
    std::printf("  one subject draws %.0f batch(es), two draw %.0f\n", alone, together);
  }

  std::printf(
      "  one subject pushes %.0f vertex uniform(s), two push %.0f\n", pushedAlone, pushedTogether);
  const bool answered = !refused.empty() || (alone > 0.0 && together >= 2.0 * alone);
  CHECK(answered,
        "**A SECOND DECLARED SUBJECT IS DRAWN, OR THE ENGINE SAYS WHY NOT**: Unreal's FScene "
        "holds a proxy per primitive and RAGE puts every entity on its node's draw list -- in "
        "neither is there a place for a second subject to be dropped from. Accepting a "
        "declaration and rendering half of it is worse than refusing it, because the frame looks "
        "finished");

  // THE UNIFORM IS PER PASS AND NOT PER SUBJECT. Batches scale with what is drawn -- that is the
  // point of a batch. The vertex uniform must not, because everything it still carries is a
  // property of the VIEW: `viewProj`, `prevViewProj`, `lightFromWorld` and the two pre-view
  // shifts. The model matrix left it for the placement buffer, so a second subject adds a ROW
  // and no push. This is the CPU term Unreal's `FGPUScene` removes and it is measured here rather
  // than asserted, because a push per model slot is invisible in a picture that looks correct.
  CHECK(refused.empty() ? pushedTogether <= pushedAlone : true,
        "**A SECOND SUBJECT COSTS A ROW, NOT A PUSH**: the vertex uniform carries only view "
        "properties now, so drawing twice as much geometry pushes it exactly as often -- the day "
        "a per-instance term returns to that uniform this number doubles and says so");

  Covers("the door: a scenario naming two subjects either draws both or is refused by name -- "
         "what it must not do is render one of them and say nothing");
  return Report();
}
