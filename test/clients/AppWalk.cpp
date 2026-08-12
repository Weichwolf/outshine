/* THE PEDESTRIAN'S FRAME, native — the entry point and the output medium, and nothing else. What it
 * shows is Outshine's; WHICH scene it shows is a mod's, and the command line is two words. */
#include <cstdio>

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
                                        "assets/world/species/beech.json", "assets/sky/moon.jpg"};

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
  Clients::Outshine app(scene, kAssets);
  Clients::ServerTelemetry telemetry(Clients::Env("OUTSHINE_SIM", "http://localhost:8080"), runId);
  /* Natively there is no browser to pin, and an invented agent string would be worse than none. */
  Clients::RunIdentity identity({id.Mod, id.Scene, id.Client, id.Build, "",
                                 scene.RenderResolution().Width, scene.RenderResolution().Height});
  app.SetTelemetryIdentity(&identity);
  app.SetTelemetrySink(&telemetry);
  app.SetTilesBase(Clients::Env("OUTSHINE_TILES", "http://localhost:8081"));
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
