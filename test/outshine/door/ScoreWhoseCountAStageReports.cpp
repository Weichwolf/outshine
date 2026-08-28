#include <cstdio>
#include <cstdlib>
#include <string>

#include <Event.h>
#include <Outshine.h>
#include <Scenario.h>

#include "Check.h"

namespace {

// A PER-FRAME COUNT BELONGS TO THE STAGE THAT MADE IT.
//
// The engine's rule is that a simulation is temporally DETERMINISTIC: the same declaration
// advanced the same way answers the same number, whatever else shares the process. A count kept
// in a mutable at process scope breaks that by construction -- two renderers in one process
// share one storage, so a number reads as whatever encoded LAST rather than what this stage did.
//
// The seam that exposes it is an early return. The shadow stage encodes nothing when no light
// stands over it:
//
//   src/render/stages/LightVisibilityStage.cpp:90
//     if (!Declared_ || Subjects_ == nullptr) { return; }
//
// so the reset that begins its count never runs. An engine whose scenario declares NO lighting
// casts nothing and must report nothing -- and against a process-scope counter it reports
// whatever the previous engine cast instead. That is the whole test:
//
//   engine A   two batches, a key declared     ->  casts 2
//   engine B   one batch,  no lighting at all  ->  casts 0, and must not inherit A's 2
//
// A stands FIRST so that a shared counter would be holding 2 at the moment B is read. The two
// subjects carry different batch counts so that inheritance is visible as a wrong NUMBER and not
// merely as a suspicious coincidence.
constexpr int kFramePx = 64;

constexpr const char *kTriangleBase64 =
    "AAAAAAAAAAAAAAAAAACAPwAAAAAAAAAAAAAAAAAAgD8AAAAAAAAAAAAAAAAAAIA/AAAAAAAAAAAAAIA/AAAAAAAAAA"
    "AAAIA/";

[[nodiscard]] std::string Held(int parts) {
  std::string nodes, meshes, materials;
  for (int at = 0; at < parts; ++at) {
    const std::string index = std::to_string(at);
    nodes += (at > 0 ? "," : "") + std::string("{\"mesh\":") + index + "}";
    meshes += (at > 0 ? "," : "") +
              std::string("{\"primitives\":[{\"attributes\":{\"POSITION\":0,\"NORMAL\":1},"
                          "\"material\":") +
              index + "}]}";
    materials += (at > 0 ? "," : "") +
                 std::string("{\"pbrMetallicRoughness\":{\"baseColorFactor\":[0.") +
                 std::to_string(at + 2) + ",0.8,0.8,1.0]}}";
  }
  std::string roots;
  for (int at = 0; at < parts; ++at) { roots += (at > 0 ? "," : "") + std::to_string(at); }
  return std::string("{\"asset\":{\"version\":\"2.0\"},\"scene\":0,\"scenes\":[{\"nodes\":[") +
         roots + "]}],\"nodes\":[" + nodes + "],\"meshes\":[" + meshes + "],\"materials\":[" +
         materials +
         "],\"accessors\":[{\"bufferView\":0,\"componentType\":5126,\"count\":3,\"type\":\"VEC3\","
         "\"min\":[0,0,0],\"max\":[1,1,0]},"
         "{\"bufferView\":1,\"componentType\":5126,\"count\":3,\"type\":\"VEC3\"}],"
         "\"bufferViews\":[{\"buffer\":0,\"byteOffset\":0,\"byteLength\":36},"
         "{\"buffer\":0,\"byteOffset\":36,\"byteLength\":36}],"
         "\"buffers\":[{\"byteLength\":72,\"uri\":\"data:application/octet-stream;base64," +
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

[[nodiscard]] outshine::Scenario Stood(const char *asset, bool lit) {
  outshine::Scenario made;
  made.Render.Declared = true;
  made.Render.Frame = outshine::Extent{kFramePx, kFramePx};
  made.Render.Fill = 0.6;
  if (lit) {
    made.Lit.Declared = true;
    made.Lit.Key.Lux = 40000.0;
    made.Lit.Key.ElevationDeg = 42.0;
    made.Lit.Key.BearingDeg = 0.0;
  }
  outshine::Asset shown;
  shown.Uri = asset;
  shown.Kind = "gltf";
  made.Assets.push_back(shown);
  return made;
}

}

int main(void) {
  using namespace outshine::Test;
  std::setvbuf(stdout, nullptr, _IONBF, 0);

  const char *nest = std::getenv("OUTSHINE_NEST");
  if (nest == nullptr || *nest == 0) {
    Unprepared("this case writes its subjects into the runner's nest and was given none");
    return Report();
  }
  const std::string under = nest;
  if (!Wrote(under + "/pair.gltf", Held(2)) || !Wrote(under + "/lone.gltf", Held(1))) {
    Unprepared("a subject could not be written into the nest");
    return Report();
  }

  if (!SDL_Init(SDL_INIT_VIDEO)) {
    Unprepared("SDL did not start, so nothing can be rendered");
    return Report();
  }

  const outshine::Roots roots{under, "src/assets", "/tmp/outshine-door-cache", true};
  outshine::Engine lit, unlit;
  lit.setRoots(roots);
  unlit.setRoots(roots);
  if (!lit.drawsInto(outshine::Extent{kFramePx, kFramePx}) ||
      !unlit.drawsInto(outshine::Extent{kFramePx, kFramePx})) {
    Unprepared("two canvases did not stand in one process");
    return Report();
  }

  if (!lit.declare(Stood("pair.gltf", true)) || !lit.advance() ||
      !lit.render(outshine::Extent{})) {
    Unprepared(("the lit engine did not stand: " + lit.error()).c_str());
    return Report();
  }
  const double litDrew = Measured(lit, "batches the picture draws");
  const double litCast = Measured(lit, "batches the shadow casts");

  if (!unlit.declare(Stood("lone.gltf", false)) || !unlit.advance() ||
      !unlit.render(outshine::Extent{})) {
    Unprepared(("the unlit engine did not stand: " + unlit.error()).c_str());
    return Report();
  }
  const double bareDrew = Measured(unlit, "batches the picture draws");
  const double bareCast = Measured(unlit, "batches the shadow casts");

  std::printf("  engine A  two parts, a key declared   draws %.0f   casts %.0f\n", litDrew,
              litCast);
  std::printf("  engine B  one part,  no lighting      draws %.0f   casts %.0f\n", bareDrew,
              bareCast);

  CHECK(litCast > 0.0 && litCast == litDrew,
        "the lit engine casts every batch it draws, so there IS a count for a second engine to "
        "inherit -- without this the check below would pass on an engine that never counted");

  CHECK(litDrew != bareDrew,
        "and the two subjects differ in the number of batches they draw, so an inherited count "
        "shows up as a WRONG NUMBER rather than as a coincidence");

  CHECK(bareCast == 0.0,
        "**A STAGE REPORTS ITS OWN COUNT**: the second engine declares no lighting, so its "
        "shadow stage returns before it encodes anything and before the reset that begins its "
        "count. It cast nothing and must say nothing. Against a mutable at process scope it "
        "answers whatever the FIRST engine cast, and a number that depends on what else shares "
        "the process is not a measurement of this frame -- the engine's determinism is a "
        "property of the declaration, not of the run order");

  Covers("the door: a per-frame count belongs to the stage instance that made it, and two "
         "engines in one process do not read each other's numbers");
  return Report();
}
