#include <cstdio>
#include <cstddef>
#include <cstring>
#include <span>
#include <string>
#include <string_view>
#include <vector>
#include <utility>

#include <SDL3/SDL.h>

#include <Logging.h>
#include <Outshine.h>
#include <scenario/Scenario.h>

#include "PlaceCamera.h"
#include "RenderAsset.h"
#include "format/Number.h"
#include <cmath>

namespace {

class Telling final : public outshine::LogSink {
public:
  void Write(double simTimeS,
             outshine::LogLevel level,
             Saying who,
             std::span<const outshine::LogField> fields) override {
    if (level == outshine::LogLevel::Debug && !Loud) { return; }
    std::printf("t=%.1f %-5s %-8s %-7s %s",
                simTimeS,
                Name(level),
                who.Unit,
                outshine::nameOf(who.Tag),
                who.Event);
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
              "worst at %zu  [sim p99 %.2f worst %.2f | draw p99 %.2f worst %.2f]\n",
              std::string(name).c_str(),
              shot.Digest.empty() ? "--------" : shot.Digest.c_str(),
              shot.P50Ms,
              shot.P95Ms,
              shot.P99Ms,
              shot.OverBudget,
              shot.Frames,
              outshine::Shots::kFrameBudgetMs,
              shot.WorstAt,
              shot.AdvanceP99Ms,
              shot.AdvanceWorstMs,
              shot.RenderP99Ms,
              shot.RenderWorstMs);
  std::printf("        %.0f triangle(s), %.0f bare tile(s), varies by %.3f of 255 along its rows; "
              "%.1f s stood, %.1f s waited (%.1f s streamed); peak heap %.0f MB; %s\n",
              shot.Triangles,
              shot.BareTiles,
              shot.VariationAlongRows,
              shot.StandingMs / 1000.0,
              shot.LoadingMs / 1000.0,
              shot.StreamedS,
              shot.PeakHeapMB,
              shot.Kept ? shot.Wrote.c_str() : "NO PICTURE");
}

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
      "  render <asset.gltf|asset.glb> <width>x<height> <output.png> [options]\n"
      "    --camera auto|index --time seconds --animation index --variant name\n"
      "    --position x,y,z --look-at x,y,z --fov degrees\n"
      "    --lighting auto|authored|studio --exposure multiplier\n"
      "  shots [--rows] [--measures] [--audit] [--all | <place>]\n"
      "                                   stand each place, draw it, keep the picture\n"
      "  places                           list the external scenario cameras\n"
      "  --places <directory> <command>   override src/assets/places\n"
      "  roundtrip                        write each place, read it back, write it again\n"
      "  run [--rows] [--into <folder>] <scenario> [name]\n"
      "                                   read a declared scenario, stand it, draw it\n"
      "  measures <scenario>              and print every measure it published\n"
      "  height <lat> <lon>               terrain elevation; angles in decimal degrees\n"
      "  help                             this\n\n"
      "Every verb is a call on `outshine::Engine`. A verb this does not have is a verb the door\n"
      "does not offer, or one nobody has needed yet.\n");
}

[[nodiscard]] bool Stands(outshine::Engine &engine,
                          outshine::Extent frame = {.WidthPx = outshine::Shots::kWidePx,
                                                    .HeightPx = outshine::Shots::kHighPx}) {
  if (!SDL_Init(SDL_INIT_VIDEO)) {
    std::printf("outshine-client: SDL did not start\n");
    return false;
  }
  engine.logsTo(&gTelling);
  engine.setRoots(outshine::Roots{.Assets = "src/assets/drive",
                                  .Shipped = "src/assets",
                                  .Cache = "/tmp/outshine-drive-cache",
                                  .Offline = false});
  if (frame.WidthPx > 0 && frame.HeightPx > 0 && !engine.drawsInto(frame)) {
    std::printf("outshine-client: the device stood no canvas -- %s\n", engine.error().c_str());
    return false;
  }
  return true;
}

int TakeShots(std::span<const Place> places, int argc, char *const *argv) {
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
    for (const Place &one : places) { taking.push_back(&one); }
  } else {
    for (int at = 0; at < argc; ++at) {
      const Place *const named = outshine::Shots::PlaceNamed(places, argv[at]);
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
      Row(shot, one->Name.c_str());
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
  if (!Stands(engine, {})) { return 2; }
  if (!engine.readScenario(argv[0])) {
    std::printf("outshine-client: %s -- %s\n", argv[0], engine.error().c_str());
    return 1;
  }
  outshine::Extent frame = engine.declaration().Render.Frame;
  if (frame.WidthPx <= 0 || frame.HeightPx <= 0) {
    frame = {.WidthPx = outshine::Shots::kWidePx, .HeightPx = outshine::Shots::kHighPx};
  }
  if (!engine.drawsInto(frame)) {
    std::printf("outshine-client: %s\n", engine.error().c_str());
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

namespace Says {
constexpr auto kHeightArguments =
    "outshine-client: height requires exactly a latitude and longitude\n";
constexpr auto kHeightCoordinates =
    "outshine-client: height requires finite latitude in [-90,90] and longitude in [-180,180]\n";
}

[[nodiscard]] int QueryTerrainHeight(std::span<char *const> arguments) {
  if (arguments.size() != 2) {
    std::fputs(Says::kHeightArguments, stderr);
    return 2;
  }
  constexpr double kLatitudeLimitDeg = 90.0;
  constexpr double kLongitudeLimitDeg = 180.0;
  const auto latitude = outshine::ParseFiniteNumber(arguments[0]);
  const auto longitude = outshine::ParseFiniteNumber(arguments[1]);
  if (!latitude || !longitude || *latitude < -kLatitudeLimitDeg || *latitude > kLatitudeLimitDeg ||
      *longitude < -kLongitudeLimitDeg || *longitude > kLongitudeLimitDeg) {
    std::fputs(Says::kHeightCoordinates, stderr);
    return 2;
  }
  const double lat = *latitude;
  const double lon = *longitude;
  outshine::Engine engine;
  if (!Stands(engine)) { return 2; }
  outshine::Scenario::Document stands;
  stands.Ground.Declared = true;
  stands.Ground.Origin.LatitudeDeg = lat;
  stands.Ground.Origin.LongitudeDeg = lon;
  stands.Ground.PatienceS = 10.0;
  if (!engine.declare(stands) || !engine.assemble() || !engine.preload(15.0)) {
    std::printf("outshine-client: the ground did not arrive -- %s\n", engine.error().c_str());
    return 1;
  }
  const outshine::Holds<double> heightM = engine.sampleHeight(
      outshine::LongitudeLatitudeHeight{.LongitudeDeg = lon, .LatitudeDeg = lat});
  if (!heightM) {
    std::printf("outshine-client: no elevation stands at %.5f %.5f -- %s\n",
                lat,
                lon,
                heightM.error().c_str());
    return 1;
  }
  std::printf("%.5f %.5f  %.2f m\n", lat, lon, *heightM);
  return 0;
}

}

int main(int argc, char **argv) {
  std::setvbuf(stdout, nullptr, _IONBF, 0);
  std::string directory = "src/assets/places";
  int argument = 1;
  if (argc > argument && std::string_view(argv[argument]) == "--places") {
    if (argc <= argument + 2) {
      std::fprintf(stderr, "outshine-client: --places requires a directory and command\n");
      return 2;
    }
    directory = argv[argument + 1];
    argument += 2;
  }
  const std::string verb = argc > argument ? argv[argument] : "help";
  const int rest = argc > argument ? argc - argument - 1 : 0;
  char *const *const from = argv + (argc > argument ? argument + 1 : argc);
  std::vector<Place> places;
  if (verb == "shots" || verb == "places" || verb == "roundtrip") {
    auto loaded = outshine::Shots::LoadPlaces(directory);
    if (!loaded) {
      std::fprintf(stderr, "outshine-client: %s\n", loaded.error().c_str());
      return 1;
    }
    places = std::move(*loaded);
  }
  if (verb == "shots") { return TakeShots(places, rest, from); }
  if (verb == "render") { return outshine::Client::RenderAsset({from, static_cast<size_t>(rest)}); }
  if (verb == "run") { return RunScenario(rest, from, false); }
  if (verb == "measures") { return RunScenario(rest, from, true); }
  if (verb == "height") { return QueryTerrainHeight({from, static_cast<std::size_t>(rest)}); }
  if (verb == "roundtrip") {
    int apart = 0;
    const std::string held = "build/outshine-roundtrip.scn";
    for (const Place &one : places) {
      outshine::Engine engine;
      if (!engine.declare(one.Declaration)) {
        std::printf(
            "APART   %-14s did not declare: %s\n", one.Name.c_str(), engine.error().c_str());
        ++apart;
        continue;
      }
      const std::string first = engine.writeScenario();
      std::FILE *const file = std::fopen(held.c_str(), "wb");
      if (file == nullptr) {
        std::printf("APART   %-14s cannot write %s\n", one.Name.c_str(), held.c_str());
        ++apart;
        continue;
      }
      std::fwrite(first.data(), 1, first.size(), file);
      std::fclose(file);
      outshine::Engine again;
      if (!again.readScenario(held)) {
        std::printf("APART   %-14s the written scenario did not read back: %s\n",
                    one.Name.c_str(),
                    again.error().c_str());
        ++apart;
        continue;
      }
      const std::string second = again.writeScenario();
      if (first == second) {
        std::printf("HELD    %-14s %zu byte(s)\n", one.Name.c_str(), first.size());
      } else {
        std::printf("APART   %-14s written twice and the two differ\n", one.Name.c_str());
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
    for (const Place &one : places) {
      const auto &view = one.Declaration.Views.front().Sees;
      const auto &at = one.Declaration.Views.front().Geographic;
      const auto &frame = one.Declaration.Render.Frame;
      std::printf("%s\t%.12g\t%.12g\t%.12g\t%.12g\t%.12g\t%.12g\t%d\t%d\t%s\n",
                  one.Name.c_str(),
                  at.Geodetic.LatitudeDeg,
                  at.Geodetic.LongitudeDeg,
                  at.Geodetic.HeightM,
                  at.BearingDeg,
                  at.PitchDeg,
                  view.FovDeg,
                  frame.WidthPx,
                  frame.HeightPx,
                  one.Declaration.Time.Start.c_str());
    }
    return 0;
  }
  Usage();
  return verb == "help" || verb == "--help" ? 0 : 2;
}
