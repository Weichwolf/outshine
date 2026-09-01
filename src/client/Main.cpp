#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <span>
#include <string>
#include <vector>

#include <SDL3/SDL.h>

#include <Logging.h>
#include <Outshine.h>
#include <Scenario.h>

#include "PlaceCamera.h"

namespace {

/// What the engine says while it works, through the door's own `logsTo`. Without a sink attached
/// every `Log::Warn` in the tree writes to nowhere, and a stall reads as a slow fetch.
class Telling final : public outshine::LogSink {
public:
  void Write(double simTimeS,
             outshine::LogLevel level,
             const char *unit,
             const char *tag,
             const char *event,
             std::span<const outshine::LogField> fields) override {
    if (level == outshine::LogLevel::Debug && !Loud) { return; }
    std::printf("t=%.1f %-5s %-8s %s", simTimeS, Name(level), unit, event);
    (void)tag;
    for (const outshine::LogField &one : fields) {
      std::printf(" %s=%s", one.Key, one.Value.c_str());
    }
    std::printf("\n");
  }

  bool Loud = false;

private:
  static const char *Name(outshine::LogLevel level) {
    switch (level) {
      case outshine::LogLevel::Debug: return "DEBUG";
      case outshine::LogLevel::Info: return "INFO";
      case outshine::LogLevel::Warn: return "WARN";
      case outshine::LogLevel::Error: return "ERROR";
    }
    return "?";
  }
};

Telling gTelling;

using outshine::Shots::Place;
using outshine::Shots::Shot;

void Tell(const Shot &shot, std::string_view name) {
  if (!shot.Why.empty()) {
    std::printf("SHOT    %-26s -- %s\n", std::string(name).c_str(), shot.Why.c_str());
    return;
  }
  std::printf("SHOT    %-26s %s  p50 %6.2f  p95 %6.2f  p99 %6.2f ms  %zu of %zu over %.2f, "
              "worst at %zu\n",
              std::string(name).c_str(),
              shot.Digest.empty() ? "--------" : shot.Digest.c_str(),
              shot.P50Ms,
              shot.P95Ms,
              shot.P99Ms,
              shot.OverBudget,
              shot.Frames,
              outshine::Shots::kFrameBudgetMs,
              shot.WorstAt);
  std::printf("        %.0f triangle(s), %.0f bare tile(s), varies by %.3f of 255 along its rows; "
              "%.1f s stood, %.1f s waited (%.1f s streamed); %s\n",
              shot.Triangles,
              shot.BareTiles,
              shot.VariationAlongRows,
              shot.StandingMs / 1000.0,
              shot.LoadingMs / 1000.0,
              shot.StreamedS,
              shot.Kept ? shot.Wrote.c_str() : "NO PICTURE");
}

/// One line a program can read: every field of a `Shot`, tab separated, in a fixed order. The
/// human-readable pair above is for an eye; this is for the case that scores it, and having both
/// means a test and a person drive the SAME command.
void Row(const Shot &shot, std::string_view name) {
  std::printf(
      "ROW\t%s\t%s\t%d\t%.4f\t%.4f\t%.4f\t%zu\t%zu\t%zu\t%.0f\t%.0f\t%.4f\t%d\t%.0f\t%.4f\t%s\n",
      std::string(name).c_str(),
      shot.Digest.empty() ? "-" : shot.Digest.c_str(),
      shot.Kept ? 1 : 0,
      shot.P50Ms,
      shot.P95Ms,
      shot.P99Ms,
      shot.Frames,
      shot.OverBudget,
      shot.WorstAt,
      shot.Triangles,
      shot.BareTiles,
      shot.VariationAlongRows,
      shot.Preloaded ? 1 : 0,
      shot.SettledOver,
      shot.PosedAtS,
      shot.Why.empty() ? "-" : shot.Why.c_str());
}

void Usage() {
  std::printf(
      "outshine-client -- the engine through its own door, from a command line.\n\n"
      "  shots [--rows] [--measures] [--audit] [--all | <place>]\n"
      "                                   stand each place, draw it, keep the picture\n"
      "  places                           name the places it knows\n"
      "  roundtrip                        write each place, read it back, write it again\n"
      "  run [--rows] [--into <folder>] <scenario> [name]\n"
      "                                   read a declared scenario, stand it, draw it\n"
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
  engine.logsTo(&gTelling);
  engine.setRoots(outshine::Roots{.Assets = "src/assets/drive",
                                  .Shipped = "src/assets",
                                  .Cache = "/tmp/outshine-drive-cache",
                                  .Offline = false});
  if (!engine.drawsInto(outshine::Extent{.WidthPx = outshine::Shots::kWidePx,
                                         .HeightPx = outshine::Shots::kHighPx})) {
    std::printf("outshine-client: the device stood no canvas -- %s\n", engine.error().c_str());
    return false;
  }
  return true;
}

int TakeShots(int argc, char *const *argv) {
  std::vector<const Place *> taking;
  bool rows = false;
  bool everyMeasure = false;
  while (argc > 0 && argv[0][0] == '-' && std::strcmp(argv[0], "--all") != 0) {
    if (std::strcmp(argv[0], "--rows") == 0) { rows = true; }
    if (std::strcmp(argv[0], "--measures") == 0) { everyMeasure = true; }
    if (std::strcmp(argv[0], "--audit") == 0) { outshine::Shots::Audits = true; }
    --argc;
    ++argv;
  }
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
  std::printf("CONTROL\t%.4f\n", outshine::Shots::ControlVariation());
  int refused = 0;
  for (const Place *const one : taking) {
    outshine::Shots::Telling = &gTelling;
    const Shot shot = outshine::Shots::Take(*one, !rows);
    if (rows) {
      Row(shot, one->Name);
    } else {
      Tell(shot, one->Name);
    }
    if (everyMeasure) {
      for (const outshine::Measure &measure : shot.Measures) {
        std::printf(
            "        %-56s %14.3f %s\n", measure.What.c_str(), measure.How, measure.Unit.c_str());
      }
    }
    refused += shot.Why.empty() && shot.Kept ? 0 : 1;
  }
  return refused == 0 ? 0 : 1;
}

int RunScenario(int argc, char *const *argv, bool everyMeasure) {
  bool rows = false;
  std::string into = "khronos";
  while (argc > 0 && argv[0][0] == '-') {
    if (std::strcmp(argv[0], "--rows") == 0) {
      rows = true;
      --argc;
      ++argv;
      continue;
    }
    if (std::strcmp(argv[0], "--into") == 0 && argc > 1) {
      into = argv[1];
      argc -= 2;
      argv += 2;
      continue;
    }
    break;
  }
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
  const Shot shot = outshine::Shots::Draw(engine, named, true, into.c_str());
  if (rows) {
    Row(shot, named.c_str());
  } else {
    Tell(shot, named);
  }
  if (everyMeasure) {
    for (const outshine::Measure &one : engine.measures()) {
      std::printf("        %-56s %14.3f %s\n", one.What.c_str(), one.How, one.Unit.c_str());
    }
  }
  return shot.Why.empty() ? 0 : 1;
}

int AskHeight(int argc, char *const *argv) {
  if (argc < 2) {
    std::printf("outshine-client: height wants a latitude and a longitude\n");
    return 2;
  }
  outshine::Engine engine;
  if (!Stands(engine)) { return 2; }
  const double lat = std::atof(argv[0]);
  const double lon = std::atof(argv[1]);
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

} // namespace

int main(int argc, char **argv) {
  std::setvbuf(stdout, nullptr, _IONBF, 0);
  const std::string verb = argc > 1 ? argv[1] : "help";
  const int rest = argc - 2;
  char **const from = argv + 2;
  if (verb == "shots") { return TakeShots(rest, from); }
  if (verb == "run") { return RunScenario(rest, from, false); }
  if (verb == "measures") { return RunScenario(rest, from, true); }
  if (verb == "height") { return AskHeight(rest, from); }
  if (verb == "roundtrip") {
    int apart = 0;
    const std::string held =
        std::string((std::getenv("TMPDIR") != nullptr) ? std::getenv("TMPDIR") : "/tmp") +
        "/outshine-roundtrip.scn";
    for (const Place &one : outshine::Shots::Places()) {
      outshine::Engine engine;
      if (!engine.declare(outshine::Shots::ScenarioFor(one))) {
        std::printf("APART   %-14s did not declare: %s\n", one.Name, engine.error().c_str());
        ++apart;
        continue;
      }
      const std::string first = engine.writeScenario();
      if (std::FILE *const file = std::fopen(held.c_str(), "wb")) {
        std::fwrite(first.data(), 1, first.size(), file);
        std::fclose(file);
      }
      outshine::Engine again;
      if (!again.readScenario(held)) {
        std::printf("APART   %-14s the written scenario did not read back: %s\n",
                    one.Name,
                    again.error().c_str());
        ++apart;
        continue;
      }
      const std::string second = again.writeScenario();
      if (first == second) {
        std::printf("HELD    %-14s %zu byte(s)\n", one.Name, first.size());
      } else {
        std::printf("APART   %-14s written twice and the two differ\n", one.Name);
        ++apart;
      }
    }
    std::printf("\n%d place(s) apart\n", apart);
    std::printf(
        "NOT COVERED: a section the WRITER drops. It is missing from the first text, so\n"
        "the second read has nothing to read and the second text matches -- measured, with\n"
        "<clock> removed every place lost 59 bytes and this still said 0 apart. The half\n"
        "it cannot see is held by a lint rule reading the grammar against the writer, and\n"
        "that rule names itself where lint prints.\n");
    return apart == 0 ? 0 : 1;
  }
  if (verb == "places") {
    for (const Place &one : outshine::Shots::Places()) {
      std::printf(
          "%-14s %10.5f %11.5f  bearing %6.2f\n", one.Name, one.LatDeg, one.LonDeg, one.BearingDeg);
    }
    return 0;
  }
  Usage();
  return verb == "help" || verb == "--help" ? 0 : 2;
}
