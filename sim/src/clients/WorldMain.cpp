/* THE SERVER TARGET, and it never opens a device. Two words in — which scenario, which scene — and
 * it answers the questions a simulation has to answer without a picture: what stands at a place,
 * how big it is, how deep the water is, and where the sun is. Its whole point is the LINK: this
 * binary contains no render/ translation unit, so an upward include is a build failure here long
 * before it is a defect anywhere else. */
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <thread>

#include "Env.h"
#include "Log.h"
#include "LogSinks.h"
#include "Mod.h"
#include "Sim.h"

using namespace outshine;

namespace {

/* A POLL, NOT A SPIN. The class grid is laid down on its own thread and the vector tiles arrive off
 * the tile pool, so the only thing this loop can do for them is get out of their way. There is no
 * ceiling: a tile server that stops answering is a fact about the server, and it gets said. */
constexpr int kPollMs = 1;
constexpr double kSaySeconds = 5.0;

void Survey(Clients::Sim &sim) {
  long passes = 0;
  auto said = std::chrono::steady_clock::now();
  while (!sim.Scenery().VectorsResident()) {
    sim.Advance();
    passes++;
    std::this_thread::sleep_for(std::chrono::milliseconds(kPollMs));
    const auto now = std::chrono::steady_clock::now();
    if (std::chrono::duration<double>(now - said).count() < kSaySeconds) continue;
    said = now;
    Log::Info("world", "surveying", {{"passes", (double)passes},
        {"vectorPending", sim.Scenery().BuildingPendingTiles()}});
  }
  Log::Info("world", "surveyed", {{"passes", (double)passes}});
}

void Answer(const Clients::Sim &sim, double lat, double lon) {
  const Clients::Sim::Place p = sim.At(lat, lon);
  const std::string cls = p.Class >= 0 && (size_t)p.Class < sim.Vegetation().TemplateCount()
                              ? sim.Vegetation().Name((size_t)p.Class)
                              : std::string("none");
  Log::Info("world", "place", {{"lat", lat}, {"lon", lon},
      {"groundResolved", p.GroundResolved}, {"groundAslM", p.GroundAslM},
      {"stands", cls}, {"classEdgeM", p.ClassEdgeM},
      {"structureHeightM", p.StructureHeightM ? *p.StructureHeightM : 0.0},
      {"structure", (bool)p.StructureHeightM},
      {"waterDepthM", p.WaterDepthM ? *p.WaterDepthM : 0.0},
      {"water", (bool)p.WaterDepthM},
      {"sunElDeg", (double)sim.SunElDeg()}, {"sunAzDeg", (double)sim.SunAzDeg()}});
}

}  // namespace

int main(int argc, char **argv) {
  if (argc < 3) {
    std::fprintf(stderr, "usage: %s <mod> <scene> [lat lon]\n", argv[0]);
    return 2;
  }
  Clients::FileLogSink sink(stdout);
  Log::SetSink(&sink);
  Clients::Mod mod;
  if (!mod.Load(Clients::Env("OUTSHINE_MODS", "../mods"), argv[1])) {
    Log::Error("world", "mod_unreadable", {{"mod", std::string(argv[1])}, {"why", mod.Error()}});
    return 2;
  }
  const Clients::Scene *scene = mod.Find(argv[2]);
  if (!scene) {
    Log::Error("world", "scene_unknown", {{"scene", std::string(argv[2])}, {"have", mod.Ids()}});
    return 2;
  }
  Clients::Sim sim(*scene, {Clients::Env("OUTSHINE_VEGETATION", "assets/world/vegetation.json"),
                            Clients::Env("OUTSHINE_MATERIALS", "assets/world/ground-materials.json"),
                            "", ""});
  sim.SetTilesBase(Clients::Env("FB_TILES", "http://localhost:8081"));
  if (!sim.LoadTables() || !sim.Open()) return 1;
  Survey(sim);
  Answer(sim, argc >= 5 ? std::atof(argv[3]) : sim.Lat(),
         argc >= 5 ? std::atof(argv[4]) : sim.Lon());
  return 0;
}
