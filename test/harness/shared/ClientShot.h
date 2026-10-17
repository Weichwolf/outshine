#ifndef OUTSHINE_TEST_CLIENTSHOT_H
#define OUTSHINE_TEST_CLIENTSHOT_H

#include <charconv>
#include <cmath>
#include <cstdio>
#include <limits>
#include <filesystem>
#include <string>
#include <vector>

#include "Check.h"
#include "src/client/GpuFrameTiming.h"

// THE CASE DRIVES THE SAME COMMAND A PERSON DOES. `build/outshine-client shots
// --rows <place>` is what `make shots` runs; a case that reached around it into the library would
// be scoring a path nobody uses. So the binary is run, its machine-readable row is read, and the
// ORACLES are applied here -- the instrument is the library's, the judgement is the test's, and
// neither is the other's.
namespace outshine::Test {

struct ClientRow {
  std::string Name;
  std::string Digest;
  std::string Why;
  bool Kept = false;
  double P50Ms = 0.0, P95Ms = 0.0, P99Ms = 0.0;
  double Frames = 0.0, OverBudget = 0.0, WorstAt = 0.0;
  double Triangles = 0.0, BareTiles = 0.0, Variation = 0.0;
  double MeasurementMs = 0.0, LoadingMs = 0.0, TurnDegrees = 0.0;
  bool Preloaded = false;
  bool Read = false;
  double WidthPx = 0.0, HeightPx = 0.0, RateHz = 0.0, BudgetMs = 0.0;
  Client::GpuFrameTiming Gpu;
};

inline double ClientNumber(const std::string &field) {
  double value = 0.0;
  const auto read = std::from_chars(field.data(), field.data() + field.size(), value);
  return read.ec == std::errc{} && read.ptr == field.data() + field.size()
             ? value
             : std::numeric_limits<double>::quiet_NaN();
}

inline void ReadClientOutput(std::FILE *running, double &control, ClientRow &row) {
  char line[4096];
  while (std::fgets(line, sizeof line, running) != nullptr) {
    std::printf("%s", line);
    std::vector<std::string> field;
    for (const char *at = line, *from = line;; ++at) {
      if (*at != '\t' && *at != '\n' && *at != '\0') { continue; }
      field.emplace_back(from, (std::size_t)(at - from));
      from = at + 1;
      if (*at != '\t') { break; }
    }
    if (field.size() >= 2 && field[0] == "CONTROL") { control = ClientNumber(field[1]); }
    if (field.size() == 6 && field[0] == "PROFILE") {
      row.WidthPx = ClientNumber(field[2]);
      row.HeightPx = ClientNumber(field[3]);
      row.RateHz = ClientNumber(field[4]);
      row.BudgetMs = ClientNumber(field[5]);
    }
    if (field.size() == 8 && field[0] == "GPUROW") {
      row.Gpu = {.RenderedFrames = ClientNumber(field[2]),
                 .TimedFrames = ClientNumber(field[3]),
                 .TotalMs = ClientNumber(field[4]),
                 .MaxMs = ClientNumber(field[5]),
                 .UnavailableFrames = ClientNumber(field[6]),
                 .QueryMs = ClientNumber(field[7])};
    }
    if (field.size() >= 20 && field[0] == "ROW") {
      row.Read = true;
      row.Name = field[1];
      row.Digest = field[2];
      row.Kept = field[3] == "1";
      row.P50Ms = ClientNumber(field[4]);
      row.P95Ms = ClientNumber(field[5]);
      row.P99Ms = ClientNumber(field[6]);
      row.Frames = ClientNumber(field[7]);
      row.OverBudget = ClientNumber(field[8]);
      row.WorstAt = ClientNumber(field[9]);
      row.Triangles = ClientNumber(field[10]);
      row.BareTiles = ClientNumber(field[11]);
      row.Variation = ClientNumber(field[12]);
      row.Preloaded = field[13] == "1";
      row.MeasurementMs = ClientNumber(field[16]);
      row.LoadingMs = ClientNumber(field[17]);
      row.TurnDegrees = ClientNumber(field[18]);
      row.Why = field.back();
    }
  }
}

inline int
ScorePlace(const char *place, const char *catalog = "src/assets/places", unsigned rateHz = 30) {
  std::setvbuf(stdout, nullptr, _IONBF, 0);
  const std::string command = std::string("build/outshine-client --places ") + catalog +
                              " shots --rows --offline --preload-seconds 600 --scenario-overrides "
                              "'{\"render\":{\"widthPx\":1280,\"heightPx\":720,\"fps\":" +
                              std::to_string(rateHz) + "}}' " + place + " 2>&1";
  std::FILE *const running = popen(command.c_str(), "r");
  if (running == nullptr) {
    Unprepared("the client did not start");
    return Report();
  }
  double control = -1.0;
  ClientRow row;
  ReadClientOutput(running, control, row);
  const int status = pclose(running);
  if (!row.Read) {
    Unprepared((std::string(place) + " left no row: the client exited " + std::to_string(status) +
                ". `make` builds it")
                   .c_str());
    return Report();
  }
  if (row.Why != "-") {
    Unprepared(row.Why.c_str());
    return Report();
  }

  constexpr double kCurrentPlaceFrameBudgetMs = 10.0;
  CHECK(status == 0, "the public client finishes successfully");
  CHECK(row.WidthPx == 1280.0 && row.HeightPx == 720.0 && row.RateHz == rateHz &&
            std::isfinite(row.BudgetMs) && std::abs(row.BudgetMs - 1000.0 / rateHz) < 1e-6,
        "the client reports the requested 1280x720 profile and its frame budget");
  CHECK(row.Frames == rateHz && row.TurnDegrees == 360.0,
        "a place measures exactly the target-rate frames of its complete turn");
  CHECK(std::isfinite(row.MeasurementMs) && row.MeasurementMs > 0.0 && row.MeasurementMs <= 1000.0,
        "the complete measured turn fits one second including pacing");
  CHECK(std::isfinite(row.LoadingMs) && row.LoadingMs >= 0.0,
        "complete cached world preload reports its actual duration");
  CHECK(row.Preloaded, "the complete world was preloaded before frame measurement");
  CHECK(std::isfinite(row.P99Ms) && row.P99Ms > 0.0 && row.P99Ms < kCurrentPlaceFrameBudgetMs,
        "current place CPU p99 stays below ten milliseconds, including initial frames");
  CHECK(row.Gpu.Complete(row.Frames),
        "every measured frame has a finite GPU timestamp; lifetime/preload data cannot substitute");
  CHECK(std::isfinite(row.Gpu.MaxMs) && row.Gpu.MaxMs > 0.0 && row.Gpu.MaxMs <= 1000.0 / rateHz,
        "GPU maximum, also nearest-rank p99 for 30 or 60 frames, fits the requested frame budget");

  // THE MEASURE'S OWN NEGATIVE CONTROL, and without it the bar below is a number nobody checked. A
  // bare ellipsoid under a sky IS a vertical gradient, so the statistic is run over one first -- if
  // THAT does not come in far under the bar, the bar separates nothing and every green is
  // worthless.
  CHECK(control >= 0.0 && control < 0.5,
        "**THE BLANK-FRAME BAR SEPARATES SOMETHING**: a picture of nothing is a vertical gradient "
        "and a vertical gradient has no horizontal variation at all, so this has to read far below "
        "the bar of 1.0 a real frame is held to");

  // A PICTURE OF NOTHING PASSED THIS CASE FOR MONTHS. A bare tile means the elevation never
  // arrived, so the ground and everything on it is drawn at sea level: the picture is of the
  // STREAMING and not of the place.
  if (row.BareTiles > 0.0) {
    Unprepared((std::string(place) + " stood " + std::to_string((long)row.BareTiles) +
                " tile(s) BARE on the ellipsoid -- the elevation never arrived, so the ground and "
                "everything on it is drawn at sea level")
                   .c_str());
    return Report();
  }
  // A PICTURE EQUAL TO A REFERENCE WAS LOOKED AT, which is the stronger proof: the bar below is a
  // heuristic against an empty frame, and a frame that is byte for byte one an eye accepted under
  // build/shots/reference/ cannot be empty -- Koerbersee's meadow and lake read 0.67 along their
  // rows with a town and a bridge in view.
  if (std::filesystem::exists(std::string("build/shots/reference/") + place + "-" + row.Digest +
                              ".png")) {
    CHECK(row.Kept,
          "**THERE IS A PICTURE, AND IT IS THE REFERENCE**: byte for byte the one an eye "
          "accepted, so no statistic about it is needed");
    Covers("a declared place on Earth stands, advances and leaves the picture its reference holds");
    return Report();
  }
  constexpr double kMinimumVisibleVariation = 0.1;
  if (row.Triangles > 0.0 && row.Variation <= control + kMinimumVisibleVariation) {
    Unprepared((std::string(place) + " meshed " + std::to_string((long)row.Triangles) +
                " building triangle(s) but varies by only " + std::to_string(row.Variation) +
                " of 255 along its rows, indistinguishable from the blank-frame control " +
                std::to_string(control))
                   .c_str());
    return Report();
  }

  CHECK(row.Kept,
        "**THERE IS A PICTURE**: the only thing this case refuses on. A place that declares, "
        "composes, advances and writes its frame has done its whole job here -- what the frame "
        "SHOWS is the owner's to judge, and no number invented here may stand in for that");

  Covers("a declared place on Earth stands, advances and leaves a picture in build/shots for an "
         "eye -- driven through build/outshine-client, the same command a person runs");
  return Report();
}

} // namespace outshine::Test
#endif
