/* THE SERVER TARGET, and it never opens a device. Two words in — which scenario, which scene — and
 * it answers the questions a simulation has to answer without a picture: what stands at a place,
 * how big it is, how deep the water is, and where the sun is. Its whole point is the LINK: this
 * binary contains no render/ translation unit, so an upward include is a build failure here long
 * before it is a defect anywhere else. */
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <thread>

#include "CurlTransport.h"
#include "Env.h"
#include "Log.h"
#include "LogSinks.h"
#include "Mod.h"
#include "Sim.h"
#include "WaterDepth.h"

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

/* The water reads as three fields and not as one metre, because the answer has three states and one
 * of them is "the level model and the DEM disagree here" (core/WaterDepth.h). */
const char *Wetness(const WaterDepth &d) {
  switch (d.Where()) {
    case WaterDepth::State::Standing: return "standing";
    case WaterDepth::State::LevelBelowGround: return "levelBelowGround";
    case WaterDepth::State::Dry: break;
  }
  return "dry";
}

std::string RowName(const Clients::Sim &sim, int row) {
  return row >= 0 && (size_t)row < sim.Vegetation().TemplateCount()
             ? sim.Vegetation().Name((size_t)row)
             : std::string("none");
}

void Answer(const Clients::Sim &sim, double lat, double lon) {
  const Clients::Sim::Place p = sim.At(lat, lon);
  double depthM = 0.0, disagreementM = 0.0;
  (void)p.Water.TryDepthM(&depthM);
  (void)p.Water.TryDisagreementM(&disagreementM);
  Log::Info("world", "place", {{"lat", lat}, {"lon", lon},
      {"groundResolved", p.GroundResolved}, {"groundAslM", p.GroundAslM},
      {"stands", RowName(sim, p.Class)}, {"classEdgeM", p.ClassEdgeM},
      {"outlinesResolved", p.OutlinesResolved},
      {"structureHeightM", p.StructureHeightM ? *p.StructureHeightM : 0.0},
      {"structure", (bool)p.StructureHeightM},
      {"made", (bool)p.Made},
      {"madeOf", p.Made ? RowName(sim, p.Made->CoverRow) : std::string("none")},
      {"wayWidthM", p.Made ? (double)p.Made->WidthM : 0.0},
      {"madeSurfaceAslM", p.Made ? p.Made->SurfaceAslM : 0.0},
      {"water", std::string(Wetness(p.Water))}, {"waterDepthM", depthM},
      {"levelBelowGroundM", disagreementM},
      {"sunElDeg", (double)sim.SunElDeg()}, {"sunAzDeg", (double)sim.SunAzDeg()}});
}

}  // namespace

int main(int argc, char **argv) {
  if (argc < 3) {
    std::fprintf(stderr, "usage: %s <mod> <scene> [lat lon]...\n", argv[0]);
    return 2;
  }
  const TextTarget logTo(TextStream::Stdout);
  Clients::TextLogSink sink(logTo);
  Log::SetSink(&sink);
  Clients::Mod mod;
  if (!mod.Load(Clients::Env("OUTSHINE_MODS", "test/mods"), argv[1])) {
    Log::Error("world", "mod_unreadable", {{"mod", std::string(argv[1])}, {"why", mod.Error()}});
    return 2;
  }
  const Clients::Scene *scene = mod.Find(argv[2]);
  if (!scene) {
    Log::Error("world", "scene_unknown", {{"scene", std::string(argv[2])}, {"have", mod.Ids()}});
    return 2;
  }
  /* BEFORE THE SIM (`C.13`): the world's tile pool borrows this wire and joins its threads in the
   * sim's destructor, so the wire has to outlive the sim rather than the other way round. */
  Host::CurlTransport wire({});
  Clients::Sim sim(*scene, {Clients::Env("OUTSHINE_VEGETATION", "src/assets/world/vegetation.json"),
                            Clients::Env("OUTSHINE_MATERIALS", "src/assets/world/ground-materials.json"),
                            "", "", "src/assets/sky/stars"});
  sim.SetTransport(wire);
  if (!sim.LoadTables()) return 1;
  /* The ground is a poll (clients/Sim.h). Natively the tile fetch behind it blocks, so this turns
   * once; the loop is what makes that a property of the transport rather than of this caller. */
  Clients::Sim::Bring open = Clients::Sim::Bring::Waiting;
  while ((open = sim.Open()) == Clients::Sim::Bring::Waiting) {}
  if (open != Clients::Sim::Bring::Open) return 1;
  Survey(sim);
  /* MANY PLACES PER RUN, because the world behind one answer costs seconds to stream and a
   * distribution over the neighbourhood is what a question about the place actually needs. */
  if (argc < 5) {
    Answer(sim, sim.Lat(), sim.Lon());
    return 0;
  }
  for (int i = 3; i + 1 < argc; i += 2) Answer(sim, std::atof(argv[i]), std::atof(argv[i + 1]));
  return 0;
}
