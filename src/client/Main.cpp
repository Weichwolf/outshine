#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

#include <SDL3/SDL.h>

#include <Outshine.h>
#include <Scenario.h>

#include "PlaceCamera.h"

namespace {

using outshine::Shots::Place;
using outshine::Shots::Shot;

void Tell(const Shot &shot, std::string_view name) {
  if (!shot.Why.empty()) {
    std::printf("SHOT    %-26s -- %s\n", std::string(name).c_str(), shot.Why.c_str());
    return;
  }
  std::printf("SHOT    %-26s %s  p50 %6.2f  p95 %6.2f  p99 %6.2f ms  %zu of %zu over %.2f, "
              "worst at %zu\n",
              std::string(name).c_str(), shot.Digest.empty() ? "--------" : shot.Digest.c_str(),
              shot.P50Ms, shot.P95Ms, shot.P99Ms, shot.OverBudget, shot.Frames,
              outshine::Shots::kFrameBudgetMs, shot.WorstAt);
  std::printf("        %.0f triangle(s), %.0f bare tile(s), varies by %.3f of 255 along its rows; "
              "%.1f s stood, %.1f s waited (%.1f s streamed); %s\n",
              shot.Triangles, shot.BareTiles, shot.VariationAlongRows, shot.StandingMs / 1000.0,
              shot.LoadingMs / 1000.0, shot.StreamedS,
              shot.Kept ? shot.Wrote.c_str() : "NO PICTURE");
}

void Usage() {
  std::printf(
      "outshine-client -- the engine through its own door, from a command line.\n\n"
      "  shots [--all | <place> ...]      stand each place, draw it, keep the picture\n"
      "  places                           name the places it knows\n"
      "  run <scenario> [name]            read a declared scenario, stand it, draw it\n"
      "  measures <scenario>              and print every measure it published\n"
      "  height <lat> <lon>               ask the Earth how high it is there\n"
      "  help                             this\n\n"
      "Every verb is a call on `outshine::Engine`. A verb this does not have is a verb the door\n"
      "does not offer, or one nobody has needed yet.\n\nPlaces:");
  for (const Place &one : outshine::Shots::Places()) { std::printf(" %s", one.Name); }
  std::printf("\n");
}

[[nodiscard]] bool Stands(outshine::Engine &engine) {
  if (!SDL_Init(SDL_INIT_VIDEO)) {
    std::printf("outshine-client: SDL did not start\n");
    return false;
  }
  engine.setRoots(
      outshine::Roots{"src/assets/drive", "src/assets", "/tmp/outshine-drive-cache", false});
  if (!engine.drawsInto(outshine::Extent{outshine::Shots::kWidePx, outshine::Shots::kHighPx})) {
    std::printf("outshine-client: the device stood no canvas -- %s\n", engine.error().c_str());
    return false;
  }
  return true;
}

int TakeShots(int argc, char **argv) {
  std::vector<const Place *> taking;
  if (argc < 1 || std::strcmp(argv[0], "--all") == 0) {
    for (const Place &one : outshine::Shots::Places()) { taking.push_back(&one); }
  } else {
    for (int at = 0; at < argc; ++at) {
      const Place *const named = outshine::Shots::PlaceNamed(argv[at]);
      if (named == nullptr) {
        std::printf("outshine-client: no place is called '%s'\n", argv[at]);
        Usage();
        return 2;
      }
      taking.push_back(named);
    }
  }
  int refused = 0;
  for (const Place *const one : taking) {
    const Shot shot = outshine::Shots::Take(*one, true);
    Tell(shot, one->Name);
    refused += shot.Why.empty() && shot.Kept ? 0 : 1;
  }
  return refused == 0 ? 0 : 1;
}

int RunScenario(int argc, char **argv, bool everyMeasure) {
  if (argc < 1) {
    std::printf("outshine-client: name a scenario to run\n");
    return 2;
  }
  const std::string named = argc > 1 ? argv[1] : "scenario";
  outshine::Engine engine;
  if (!Stands(engine)) { return 2; }
  if (!engine.readScenario(argv[0])) {
    std::printf("outshine-client: %s -- %s\n", argv[0], engine.error().c_str());
    return 1;
  }
  if (!engine.assemble()) {
    std::printf("outshine-client: %s did not assemble -- %s\n", argv[0], engine.error().c_str());
    return 1;
  }
  const Shot shot = outshine::Shots::Draw(engine, named, true);
  Tell(shot, named);
  if (everyMeasure) {
    for (const outshine::Measure &one : engine.measures()) {
      std::printf("        %-56s %14.3f %s\n", one.What.c_str(), one.How, one.Unit.c_str());
    }
  }
  return shot.Why.empty() ? 0 : 1;
}

int AskHeight(int argc, char **argv) {
  if (argc < 2) {
    std::printf("outshine-client: height wants a latitude and a longitude\n");
    return 2;
  }
  outshine::Engine engine;
  if (!Stands(engine)) { return 2; }
  const double lat = std::atof(argv[0]), lon = std::atof(argv[1]);
  outshine::Scenario stands;
  stands.Ground.Declared = true;
  stands.Ground.Origin.LatitudeDeg = lat;
  stands.Ground.Origin.LongitudeDeg = lon;
  stands.Ground.PatienceS = 10.0;
  if (!engine.declare(stands) || !engine.assemble() || !engine.preload(15.0)) {
    std::printf("outshine-client: the ground did not arrive -- %s\n", engine.error().c_str());
    return 1;
  }
  double heightM = 0.0;
  if (!engine.sampleHeight(lat, lon, heightM)) {
    std::printf("outshine-client: no elevation stands at %.5f %.5f\n", lat, lon);
    return 1;
  }
  std::printf("%.5f %.5f  %.2f m\n", lat, lon, heightM);
  return 0;
}

}

int main(int argc, char **argv) {
  std::setvbuf(stdout, nullptr, _IONBF, 0);
  const std::string verb = argc > 1 ? argv[1] : "help";
  const int rest = argc - 2;
  char **from = argv + 2;
  if (verb == "shots") { return TakeShots(rest, from); }
  if (verb == "run") { return RunScenario(rest, from, false); }
  if (verb == "measures") { return RunScenario(rest, from, true); }
  if (verb == "height") { return AskHeight(rest, from); }
  if (verb == "places") {
    for (const Place &one : outshine::Shots::Places()) {
      std::printf("%-14s %10.5f %11.5f  bearing %6.2f\n", one.Name, one.LatDeg, one.LonDeg,
                  one.BearingDeg);
    }
    return 0;
  }
  Usage();
  return verb == "help" || verb == "--help" ? 0 : 2;
}
