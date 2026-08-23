#include <cstdio>
#include <string>
#include <vector>

#include "Check.h"

#include "CurlTransport.h"
#include "LogSinks.h"
#include "Mod.h"
#include "Sim.h"

namespace {

constexpr const char *kMod = "test/unit/scenario/mods/demo/mod.json";
constexpr const char *kScene = "town";
constexpr int kAdvances = 4000;

std::string Slurp(const char *path, bool &read) {
  std::FILE *const file = std::fopen(path, "rb");
  if (file == nullptr) { read = false; return std::string(); }
  std::string text;
  int c = 0;
  while ((c = std::fgetc(file)) != EOF) { text.push_back((char)c); }
  std::fclose(file);
  read = true;
  return text;
}

} // namespace

int main(void) {
  using namespace outshine::Test;
  using outshine::Clients::Sim;
  std::setvbuf(stdout, nullptr, _IONBF, 0);

  const outshine::TextTarget target(outshine::TextStream::Stdout);
  outshine::Clients::TextLogSink sink(target);
  const outshine::Clients::LogSinkScope logging(&sink);

  bool read = false;
  const std::string text = Slurp(kMod, read);
  CHECK(read, "the declared world reads from the tree");
  if (!read) { return Report(); }

  outshine::SceneLegacy::Mod mod;
  std::string error;
  const bool parsed = mod.Read(text, kMod);
  if (!parsed) { std::printf("REFUSED %s\n", mod.Error().c_str()); }
  CHECK(parsed, "and parses");
  if (!parsed) { return Report(); }

  const outshine::SceneLegacy::Scene *const scene = mod.Find(kScene);
  CHECK(scene != nullptr, "and the scene it names is in it");
  if (scene == nullptr) { return Report(); }

  Sim::Assets assets;
  assets.Vegetation = "src/assets/world/vegetation.json";
  assets.GroundMaterials = "src/assets/world/ground-materials.json";
  assets.Species = "src/assets/world/species/beech.json";
  assets.Moon = "src/assets/sky/moon.jpg";
  assets.Stars = "src/assets/sky";

  Sim sim(*scene, assets);
  const bool tables = sim.LoadTables();
  CHECK(tables, "**THE TABLES THE WORLD IS MADE OF LOAD.** Ground materials and the vegetation "
                "table are content, read from src/assets, and the world is a function of them");
  if (!tables) { return Report(); }

  outshine::Host::CurlTransport::Config wiring;
  outshine::Host::CurlTransport wire(wiring);
  sim.SetTransport(wire);

  Sim::Bring brought = Sim::Bring::Waiting;
  int advanced = 0;
  for (; advanced < kAdvances && brought == Sim::Bring::Waiting; ++advanced) {
    brought = sim.Open();
    if (brought == Sim::Bring::Waiting) { sim.Advance(); }
  }
  Note("advances the world took to open", (double)advanced, "advances");
  CHECK(brought == Sim::Bring::Open,
        "**AND A WORLD STANDS UP WHERE THE SCENE SAYS IT DOES.** Terrain from the elevation data, "
        "OSM's own ways and footprints, vegetation from the tables and a sky on the scene's own "
        "clock -- this is the first case in the tree that constructs a Sim at all");
  if (brought != Sim::Bring::Open) { return Report(); }

  const outshine::SceneLegacy::WorldStage *const staged = scene->Staged().AsWorld();
  CHECK(staged != nullptr, "the scene stages a world rather than a studio");
  if (staged == nullptr) { return Report(); }

  const Sim::Place stood = sim.At(staged->Where.LatDeg(), staged->Where.LonDeg());
  Note("the ground the world resolves under the standpoint", stood.GroundAslM, "m asl");
  CHECK(stood.GroundResolved,
        "**AND THE GROUND UNDER THE STANDPOINT IS A NUMBER.** Not a default and not a plane: the "
        "elevation the terrain provider carries for that coordinate");
  CHECK(stood.GroundAslM > 50.0 && stood.GroundAslM < 120.0,
        "and it is the Weser valley's -- Hameln's old town sits near 68 m, and the bound is the "
        "valley floor and the ridge above it rather than a number tuned to what came out");

  Note("the sun's elevation on the scene's clock", (double)sim.SunElDeg(), "deg");
  Note("its bearing", (double)sim.SunAzDeg(), "deg");
  CHECK(sim.SunElDeg() > 5.0 && sim.SunElDeg() < 20.0 && sim.SunAzDeg() > 250.0 &&
            sim.SunAzDeg() < 300.0,
        "**AND THE SUN IS WHERE THE CLOCK PUTS IT.** 17:40 UTC on 6 August at 52.1 N is 19:40 "
        "local summer time, so the sun is LOW and in the west -- 11.25 deg at a bearing of 282.5, "
        "which is evening light and not the afternoon it reads like. An ephemeris answers this; a "
        "declared elevation cannot, and a day and night cycle is exactly the thing that needs it");

  Covers("I.4.4 a declared world stands up from upstream data alone: terrain, OSM, vegetation and a "
         "sky on the scene's own clock, with the ground under the standpoint resolved to a number");
  return Report();
}
