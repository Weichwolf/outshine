/* THE PEDESTRIAN'S FRAME, native — the entry point and the output medium, and nothing else. What it
 * shows is Outshine's; WHICH scene it shows is a mod's, and the command line is two words. */
#include <cstdio>
#include <cstdlib>

#include "CsvTelemetry.h"
#include "CurlTransport.h"
#include "DelayedTransport.h"
#include "Env.h"
#include "FileArtifacts.h"
#include "Log.h"
#include "LogSinks.h"
#include "Mod.h"
#include "RunIdentity.h"
#include "Sanitisers.h"
#include "SceneRunner.h"
#include "Snapshot.h"
#include "Stage.h"
#include "TextTarget.h"

/* WHICH BINARY THIS IS, from the build that names the binary. A literal here was wrong for every
 * build but one, and it wrote `gpu_walk` into the archive out of `build/gpu_walk_asan`. */
#ifndef OUTSHINE_CLIENT
#error "the build names the client: -DOUTSHINE_CLIENT=\"...\""
#endif

using namespace outshine;

namespace {

/* The engine's own declarations, by path, relative to the working directory. Nothing here is
 * content. */
const Clients::Outshine::Assets kAssets{"src/assets/world/vegetation.json",
                                        "src/assets/world/ground-materials.json",
                                        "src/assets/world/species/beech.json", "src/assets/sky/moon.jpg",
                                        "src/assets/sky/stars"};

/* WHERE THIS MACHINE KEEPS A RUN'S PRODUCTS. Named once: the artefacts and the telemetry file are
 * two things in the same place, and two reads of the variable are two statements. */
[[nodiscard]] std::string ArtifactRoot() { return Clients::Env("OUTSHINE_OUT", "."); }

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
[[nodiscard]] bool Stand(const Scenario::Scene &scene, Clients::Outshine &app) {
  const Scenario::WorldStage *world = scene.Staged().AsWorld();
  if (!world || world->SnapshotPath.empty()) return true;
  Clients::Snapshot snap;
  if (!snap.Load(world->SnapshotPath.c_str()) || !snap.Matches(*world, scene.FovDeg())) {
    Log::Error("run", "snapshot_refused", {{"path", world->SnapshotPath}, {"why", snap.Error()}});
    return false;
  }
  app.SetStance({snap.Lat(), snap.Lon(), snap.YawDeg(), snap.PitchDeg()});
  Log::Info("run", "snapshot", {{"name", snap.Name()}, {"client", snap.Client()},
      {"lat", snap.Lat()}, {"lon", snap.Lon()}, {"yawDeg", snap.YawDeg()},
      {"pitchDeg", snap.PitchDeg()}});
  return true;
}

/* WHERE THIS RUN'S TEXT GOES, and only the consumer may say: `stdout`, `stderr` or a path. */
[[nodiscard]] TextTarget Named(const char *variable, const std::string &fallback) {
  const std::string where = Clients::Env(variable, fallback.c_str());
  if (where == "stdout") return TextTarget(TextStream::Stdout);
  if (where == "stderr") return TextTarget(TextStream::Stderr);
  return TextTarget(where);
}

/* On stderr and nowhere else, because the destination that failed may be the log itself. */
[[nodiscard]] bool Writable(const TextTarget &target, const char *channel) {
  if (target.Refusal().empty()) return true;
  fprintf(stderr, "%s: %s\n", channel, target.Refusal().c_str());
  return false;
}

int Record(const Scenario::Scene &scene, Clients::RunIdentity::Fields fields,
           const TextTarget &telemetryTo) {
  /* BEFORE THE APP (`C.13`): the world's tile pool borrows this wire and joins its threads in the
   * app's destructor, so the wire has to outlive the app rather than the other way round. */
  Host::CurlTransport wire({});
  Host::DelayedTransport ordered(wire, DeclaredDelay());
  Clients::Outshine app(scene, kAssets);
  Clients::CsvTelemetry telemetry(telemetryTo);
  fields.RenderW = scene.RenderResolution().Width;
  fields.RenderH = scene.RenderResolution().Height;
  Clients::RunIdentity identity(fields);
  app.SetTelemetryIdentity(&identity);
  app.SetTelemetrySink(&telemetry);
  /* A STUDIO REACHES NO UPSTREAM AND KEEPS NO CACHE, and it is the stage that says so: a wire handed
   * to a scene with no place is a wire that can only be used by mistake. */
  if (scene.Staged().AsWorld()) {
    app.SetTransport(ordered);
    app.SetContentStore(DeclaredStore());
  }
  if (!Stand(scene, app)) return 1;
  if (!app.Prepare()) return 1;

  Clients::FileArtifacts out(ArtifactRoot());
  Clients::SceneRunner runner(app, scene, out);
  while (runner.Step() == Clients::SceneRunner::Progress::Running) {}
  return runner.Result();
}

/* THE FIRST LINE NAMES THE RUN, so a log cut out of a directory still says what produced it and
 * where its numbers went. The instrument is not the caller's to state (Sanitisers.h). */
void Announce(const Clients::RunIdentity::Fields &fields, const TextTarget &logTo,
              const TextTarget &telemetryTo) {
  Log::Info("run", "identity", {{"mod", fields.Mod}, {"scene", fields.Scene},
      {"client", fields.Client}, {"san", std::string(Clients::kSanitisers)},
      {"build", fields.Build.empty() ? std::string("unset") : fields.Build},
      {"host", Clients::Env("HOSTNAME", "")}, {"log", logTo.Name()},
      {"telemetry", telemetryTo.Name()}});
}

}  // namespace

int main(int argc, char **argv) {
  if (argc != 3) {
    fprintf(stderr, "usage: %s <mod> <scene>\n", argv[0]);
    return 2;
  }
  const TextTarget logTo = Named("OUTSHINE_LOG", "stdout");
  if (!Writable(logTo, "log")) return 1;
  const TextTarget telemetryTo = Named("OUTSHINE_TELEMETRY", ArtifactRoot() + "/telemetry.csv");
  if (!Writable(telemetryTo, "telemetry")) return 1;
  Clients::TextLogSink sink(logTo);
  Clients::LogSinkScope scope(&sink);
  Log::SetLevel(LogLevel::Debug);
  /* Natively there is no browser to pin, and an invented agent string would be worse than none. */
  const Clients::RunIdentity::Fields fields{argv[1], argv[2], OUTSHINE_CLIENT,
                                            Clients::Env("OUTSHINE_BUILD", ""), "", 0, 0};
  Announce(fields, logTo, telemetryTo);

  Scenario::Mod mod;
  if (!mod.Load(Clients::Env("OUTSHINE_MODS", "test/mods"), argv[1])) {
    Log::Error("run", "mod_load_failed", {{"why", mod.Error()}});
    return 1;
  }
  const Scenario::Scene *scene = mod.Find(argv[2]);
  if (!scene) {
    Log::Error("run", "unknown_scene", {{"mod", mod.Name()}, {"asked", std::string(argv[2])},
        {"has", mod.Ids()}});
    return 1;
  }
  if (scene->What() != Scenario::Scene::Kind::Run) {
    Log::Error("run", "scene_is_interactive", {{"scene", scene->Id()}});
    return 1;
  }
  return Record(*scene, fields, telemetryTo);
}
