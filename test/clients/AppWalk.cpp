/* THE PEDESTRIAN'S FRAME, native — the entry point and the output medium, and nothing else. What it
 * shows is Outshine's; WHICH scene it shows is a mod's, and the command line is two words. */
#include <cstdio>
#include <cstdlib>

#include "CurlTransport.h"
#include "DelayedTransport.h"
#include "Env.h"
#include "FileArtifacts.h"
#include "Log.h"
#include "LogSinks.h"
#include "Mod.h"
#include "RunIdentity.h"
#include "SceneRunner.h"
#include "ServerLog.h"
#include "ServerTelemetry.h"
#include "Snapshot.h"

/* WHICH BINARY THIS IS, from the build that names the binary. A literal here was wrong for every
 * build but one, and it wrote `gpu_walk` into the archive out of `build/gpu_walk_asan`. */
#ifndef OUTSHINE_CLIENT
#error "the build names the client: -DOUTSHINE_CLIENT=\"...\""
#endif

using namespace outshine;

namespace {

/* The engine's own declarations, by path, because the two toolchains mount them differently — a
 * preloaded virtual FS in the browser, the working directory natively. Nothing here is content. */
const Clients::Outshine::Assets kAssets{"assets/world/vegetation.json",
                                        "assets/world/ground-materials.json",
                                        "assets/world/species/beech.json", "assets/sky/moon.jpg",
                                        "assets/sky/stars"};

/* THE CONTENT STORE IS THE RUN'S DECISION and never the library's: cache-on and cache-off differ in
 * timing and in nothing else, and the still gate turns it off because an imposed arrival order needs
 * the arrivals to actually happen. */
[[nodiscard]] Data::ContentStore::Config DeclaredStore() {
  Data::ContentStore::Config store;
  store.Directory = Clients::Env("OUTSHINE_CONTENT", "");
  if (!Clients::Env("OUTSHINE_NO_CONTENT_STORE", "").empty())
    store.Using = Data::ContentStore::Use::Off;
  return store;
}

/* THE ARRIVAL ORDER, IMPOSED. Unset means the host's own order, which is what a person walking gets;
 * a seed makes the order a declared input, which is what the still gate needs. */
[[nodiscard]] Host::DelayedTransport::Config DeclaredDelay() {
  Host::DelayedTransport::Config delay;
  delay.Seed = (uint32_t)atoi(Clients::Env("OUTSHINE_ARRIVAL_SEED", "0").c_str());
  delay.SpreadMs = delay.Seed == 0 ? 0 : atoi(Clients::Env("OUTSHINE_ARRIVAL_SPREAD_MS", "400").c_str());
  return delay;
}

/* A STANDPOINT SOMEONE ELSE STOOD AT (Snapshot.h): lat/lon/yaw/pitch out of one line of fb-sim's
 * shots.jsonl, refused if the scene it names is not this one. It REPLACES the declared standpoint
 * rather than combining with it — two statements of where the eye is, applied at once, describe a
 * picture neither of them means. */
[[nodiscard]] bool Stand(const Clients::Scene &scene, Clients::Outshine &app) {
  if (scene.Snapshot().empty()) return true;
  Clients::Snapshot snap;
  if (!snap.Load(scene.Snapshot().c_str()) || !snap.Matches(scene)) {
    Log::Error("run", "snapshot_refused", {{"path", scene.Snapshot()}, {"why", snap.Error()}});
    return false;
  }
  app.SetStance({snap.Lat(), snap.Lon(), snap.YawDeg(), snap.PitchDeg()});
  Log::Info("run", "snapshot", {{"name", snap.Name()}, {"client", snap.Client()},
      {"lat", snap.Lat()}, {"lon", snap.Lon()}, {"yawDeg", snap.YawDeg()},
      {"pitchDeg", snap.PitchDeg()}});
  return true;
}

int Record(const Clients::Scene &scene, const Clients::ServerLog::Identity &id,
           const std::string &runId) {
  /* BEFORE THE APP (`C.13`): the world's tile pool borrows this wire and joins its threads in the
   * app's destructor, so the wire has to outlive the app rather than the other way round. */
  Host::CurlTransport wire({});
  Host::DelayedTransport ordered(wire, DeclaredDelay());
  Clients::Outshine app(scene, kAssets);
  Clients::ServerTelemetry telemetry(Clients::Env("OUTSHINE_SIM", "http://localhost:8080"), runId);
  /* Natively there is no browser to pin, and an invented agent string would be worse than none. */
  Clients::RunIdentity identity({id.Mod, id.Scene, id.Client, id.Build, "",
                                 scene.RenderResolution().Width, scene.RenderResolution().Height});
  app.SetTelemetryIdentity(&identity);
  app.SetTelemetrySink(&telemetry);
  app.SetTransport(ordered);
  app.SetContentStore(DeclaredStore());
  if (!Stand(scene, app)) return 1;
  if (!app.Prepare({nullptr})) return 1;

  Clients::FileArtifacts out(Clients::Env("OUTSHINE_OUT", "."));
  Clients::SceneRunner runner(app, scene, out);
  /* THE HOST'S TURN IS THE WHOLE DIFFERENCE between the two clients. Here the process owns the
   * thread and a turn follows a turn; in the browser a turn is a task, and the run's sequence is
   * the same object either way. */
  while (runner.Step() == Clients::SceneRunner::Progress::Running) {}
  return runner.Result();
}

}  // namespace

int main(int argc, char **argv) {
  if (argc != 3) {
    fprintf(stderr, "usage: %s <mod> <scene>\n", argv[0]);
    return 2;
  }
  Clients::StdoutLogSink console;
  const Clients::ServerLog::Identity id{argv[1], argv[2], OUTSHINE_CLIENT,
                                        Clients::Env("OUTSHINE_BUILD", ""),
                                        Clients::Env("HOSTNAME", "")};
  Clients::ServerLog server(Clients::Env("OUTSHINE_SIM", "http://localhost:8080"), id);
  Clients::CompositeLogSink both;
  both.Add(&console);
  both.Add(&server);
  Clients::LogSinkScope scope(&both);
  Log::SetLevel(LogLevel::Debug);

  Clients::Mod mod;
  if (!mod.Load(Clients::Env("OUTSHINE_MODS", "mods"), argv[1])) {
    Log::Error("run", "mod_load_failed", {{"why", mod.Error()}});
    return 1;
  }
  const Clients::Scene *scene = mod.Find(argv[2]);
  if (!scene) {
    Log::Error("run", "unknown_scene", {{"mod", mod.Name()}, {"asked", std::string(argv[2])},
        {"has", mod.Ids()}});
    return 1;
  }
  if (scene->What() != Clients::Scene::Kind::Run) {
    Log::Error("run", "scene_is_interactive", {{"scene", scene->Id()}});
    return 1;
  }
  return Record(*scene, id, server.RunId());
}
