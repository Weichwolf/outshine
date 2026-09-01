#include "PlaceCamera.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <ratio>
#include <span>
#include <string_view>
#include <system_error>
#include <array>
#include <chrono>
#include <cstdio>
#include <numbers>
#include <string>
#include <filesystem>

#include <SDL3/SDL.h>

#include <Outshine.h>
#include <scenario/Scenario.h>
#include <vector>

#include "Sha256.h"

namespace outshine::Shots {
namespace {

constexpr double kPatienceS = 15.0;
constexpr double kSightM = 240000.0;
constexpr double kEyeAglM = 60.0;
constexpr double kPlanAboveM = 4000.0;
constexpr double kPitchDeg = -6.0;
constexpr double kFovDeg = 55.0;
constexpr int kTimedFrames = 120;

/// THE FRAME BUDGET IS JUDGED OVER A MOVING CAMERA, which CLAUDE.md's aim says in those words and
/// this instrument did not do: it timed 120 frames of one still view. A still camera is the case a
/// renderer is best at -- it never rebuilds the world, never streams a tile it has not got. The
/// path is DECLARED as views the scenario carries and stepped through with `setView`, so the
/// picture is untouched: it is written before the timed loop and always from `station`.
constexpr int kWalkViews = 24;
constexpr double kWalkStepM = 25.0;

void WalkedTo(
    double fromLat, double fromLon, double bearingDeg, double alongM, double &lat, double &lon) {
  constexpr double kMetresPerDegree = 111320.0;
  const double heading = bearingDeg * std::numbers::pi / 180.0;
  lat = fromLat + alongM * std::cos(heading) / kMetresPerDegree;
  const double shrink = std::cos(fromLat * std::numbers::pi / 180.0);
  lon =
      fromLon + alongM * std::sin(heading) / (kMetresPerDegree * (shrink > 1.0e-6 ? shrink : 1.0));
}

constexpr std::array<Place, 9> kPlaces{{
    {.Name = "OldTown",
     .LatDeg = 49.3777,
     .LonDeg = 10.179,
     .BearingDeg = 70.0,
     .From = Place::Seen::Eye,
     .SpanM = 0.0,
     .WhenUtc = "2026-06-21T11:19:00Z"},
    {.Name = "Heidelberg",
     .LatDeg = 49.4147,
     .LonDeg = 8.6968,
     .BearingDeg = 108.50,
     .From = Place::Seen::Eye,
     .SpanM = 0.0,
     .WhenUtc = "2026-06-21T11:25:00Z"},
    {.Name = "Shibuya",
     .LatDeg = 35.6595,
     .LonDeg = 139.7005,
     .BearingDeg = 40.0,
     .From = Place::Seen::Eye,
     .SpanM = 0.0,
     .WhenUtc = "2026-06-21T02:41:00Z"},
    {.Name = "CentralPark",
     .LatDeg = 40.7968,
     .LonDeg = -73.9520,
     .BearingDeg = 218.32,
     .From = Place::Seen::Eye,
     .SpanM = 0.0,
     .WhenUtc = "2026-06-21T16:56:00Z"},
    {.Name = "Venice",
     .LatDeg = 45.438,
     .LonDeg = 12.3358,
     .BearingDeg = 30.0,
     .From = Place::Seen::Eye,
     .SpanM = 0.0,
     .WhenUtc = "2026-06-21T11:11:00Z"},
    {.Name = "Jura",
     .LatDeg = 47.2492,
     .LonDeg = 7.5108,
     .BearingDeg = 156.53,
     .From = Place::Seen::Eye,
     .SpanM = 0.0,
     .WhenUtc = "2026-06-21T11:30:00Z"},
    {.Name = "ZurichPlan",
     .LatDeg = 47.3667,
     .LonDeg = 8.5500,
     .BearingDeg = 0.0,
     .From = Place::Seen::Plan,
     .SpanM = 3000.0,
     .WhenUtc = "2026-06-21T11:26:00Z"},
    {.Name = "Kaiserberg",
     .LatDeg = 51.4400,
     .LonDeg = 6.8040,
     .BearingDeg = 45.0,
     .From = Place::Seen::Eye,
     .SpanM = 0.0,
     .WhenUtc = "2026-06-21T11:32:00Z"},
    {.Name = "Koehlbrand",
     .LatDeg = 53.5195,
     .LonDeg = 9.9205,
     .BearingDeg = 58.0,
     .From = Place::Seen::Eye,
     .SpanM = 0.0,
     .WhenUtc = "2026-06-21T11:34:00Z"},
}};

} // namespace

std::span<const Place> Places() {
  return kPlaces;
}

const Place *PlaceNamed(std::string_view name) {
  for (const Place &one : kPlaces) {
    if (name == one.Name) { return &one; }
  }
  return nullptr;
}

double VariationAlongRows(std::span<const std::uint8_t> rgba, int wide, int high) {
  if (wide < 2 || high < 1 ||
      rgba.size() < static_cast<std::size_t>(wide) * static_cast<std::size_t>(high) * 4u) {
    return 0.0;
  }
  double sum = 0.0;
  std::size_t steps = 0;
  for (int y = 0; y < high; ++y) {
    for (int x = 1; x < wide; ++x) {
      const std::size_t at = (static_cast<std::size_t>(y) * static_cast<std::size_t>(wide) +
                              static_cast<std::size_t>(x)) *
                             4u;
      for (int channel = 0; channel < 3; ++channel) {
        const int here = rgba[at + static_cast<std::size_t>(channel)];
        const int left = rgba[at - 4u + static_cast<std::size_t>(channel)];
        sum += here > left ? here - left : left - here;
        ++steps;
      }
    }
  }
  return steps > 0 ? sum / static_cast<double>(steps) : 0.0;
}

double ControlVariation() {
  std::vector<std::uint8_t> gradient(
      static_cast<std::size_t>(kWidePx) * static_cast<std::size_t>(kHighPx) * 4u, 255u);
  for (int y = 0; y < kHighPx; ++y) {
    const auto shade = static_cast<std::uint8_t>(255 * y / (kHighPx - 1));
    for (int x = 0; x < kWidePx; ++x) {
      const std::size_t at = (static_cast<std::size_t>(y) * static_cast<std::size_t>(kWidePx) +
                              static_cast<std::size_t>(x)) *
                             4u;
      gradient[at] = shade;
      gradient[at + 1] = shade;
      gradient[at + 2] = static_cast<std::uint8_t>(255 - shade);
    }
  }
  return VariationAlongRows(gradient, kWidePx, kHighPx);
}

LogSink *Telling = nullptr;
bool Audits = false;

Scenario::Document ScenarioFor(const Place &place) {
  Scenario::Document stands;
  stands.Ground.Declared = true;
  stands.Ground.Origin.LatitudeDeg = place.LatDeg;
  stands.Ground.Origin.LongitudeDeg = place.LonDeg;
  stands.Ground.PatienceS = 3.0;
  stands.Ground.SightM = kSightM;
  stands.Render.Declared = true;
  stands.Render.Frame = Extent{.WidthPx = kWidePx, .HeightPx = kHighPx};
  stands.Render.Fill = 0.6;
  stands.Render.Audits = Audits;
  stands.Lit.Declared = true;
  stands.Time.Declared = true;
  stands.Time.Live = false;
  stands.Time.Start = place.WhenUtc;

  Scenario::View watches;
  watches.Id = "station";
  watches.Person = "first";
  watches.Sees.Stands.GlobeAnchor = true;
  watches.Sees.Stands.Geodetic.LatitudeDeg = place.LatDeg;
  watches.Sees.Stands.Geodetic.LongitudeDeg = place.LonDeg;
  const bool overhead = place.From == Place::Seen::Plan;
  watches.Sees.Stands.Geodetic.HeightM = overhead ? kPlanAboveM : kEyeAglM;
  watches.Sees.Stands.SamplesHeight = !overhead;
  watches.Sees.Stands.BearingDeg = place.BearingDeg;
  watches.Sees.Stands.PitchDeg = overhead ? -90.0 : kPitchDeg;
  watches.Sees.FovDeg = kFovDeg;
  if (overhead) {
    watches.Sees.Orthographic = true;
    watches.Sees.YMagM = 0.5 * place.SpanM;
    watches.Sees.XMagM =
        0.5 * place.SpanM * static_cast<double>(kWidePx) / static_cast<double>(kHighPx);
    watches.Sees.NearM = 1.0;
    watches.Sees.FarM = kPlanAboveM * 2.0;
  }
  stands.Views.push_back(watches);

  for (int step = 1; step <= kWalkViews; ++step) {
    Scenario::View along = watches;
    along.Id = "walk" + std::to_string(step - 1);
    double lat = place.LatDeg;
    double lon = place.LonDeg;
    WalkedTo(place.LatDeg,
             place.LonDeg,
             place.BearingDeg,
             static_cast<double>(step) * kWalkStepM,
             lat,
             lon);
    along.Sees.Stands.Geodetic.LatitudeDeg = lat;
    along.Sees.Stands.Geodetic.LongitudeDeg = lon;
    stands.Views.push_back(along);
  }
  return stands;
}

Shot Take(const Place &place, bool tells) {
  Shot shot;
  if (!SDL_Init(SDL_INIT_VIDEO)) {
    shot.Why = "SDL did not start, so nothing can be drawn";
    return shot;
  }
  Engine engine;
  if (Telling != nullptr) { engine.logsTo(Telling); }
  engine.setRoots(Roots{.Assets = "src/assets/drive",
                        .Shipped = "src/assets",
                        .Cache = "/tmp/outshine-drive-cache",
                        .Offline = false});
  if (!engine.drawsInto(Extent{.WidthPx = kWidePx, .HeightPx = kHighPx})) {
    shot.Why = "the device stood no canvas";
    return shot;
  }

  const Scenario::Document stands = ScenarioFor(place);

  const auto began = std::chrono::steady_clock::now();
  if (!engine.declare(stands) || !engine.assemble()) {
    shot.Why = std::string(place.Name) +
               " needs terrain and OSM tiles and this machine has none "
               "cached: " +
               engine.error();
    return shot;
  }
  const double stoodMs =
      std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - began).count();
  Shot drawn = Draw(engine, place.Name, tells);
  drawn.StandingMs = stoodMs;
  return drawn;
}

Shot Draw(Engine &engine, std::string_view name, bool tells, std::string_view under) {
  Shot shot;
  const auto asked = std::chrono::steady_clock::now();
  Loading last;
  const bool ready = engine
                         .preload(kPatienceS,
                                  [&](const Loading &how) {
                                    if (!tells) { return; }
                                    if (how.ElapsedS - last.ElapsedS < 0.25 && how.share() < 1.0) {
                                      return;
                                    }
                                    last = how;
                                    std::printf("\r    loading  terrain %zu/%zu  osm %zu/%zu  "
                                                "%zu in flight  %.1f MB  %.0f Mbit/s  %.1f s   ",
                                                how.GroundArrived,
                                                how.GroundWanted,
                                                how.VectorArrived,
                                                how.VectorWanted,
                                                how.Outstanding,
                                                how.FetchedMB,
                                                how.Megabits,
                                                how.ElapsedS);
                                    std::fflush(stdout);
                                  })
                         .has_value();
  if (tells) { std::printf("\n"); }

  const auto stood = std::chrono::steady_clock::now();
  shot.StreamedS = last.ElapsedS;
  shot.Preloaded = ready;
  shot.LoadingMs = std::chrono::duration<double, std::milli>(stood - asked).count();

  const int settle = engine.renderer().settleFrames();
  const int wanted = settle > 2 ? settle : 2;
  for (int at = 0; at < wanted; ++at) {
    if (!engine.advance()) {
      shot.Why = std::string(name) + " did not advance: " + engine.error();
      return shot;
    }
    if (!engine.renderer().render(Extent{})) {
      shot.Why = std::string(name) + " did not render: " + engine.error();
      return shot;
    }
  }

  const auto measured = [&engine](const char *what) {
    for (const Measure &held : engine.measures()) {
      if (held.What == what) { return held.How; }
    }
    return 0.0;
  };
  shot.Triangles = measured("building triangles the world meshed");
  shot.BareTiles = measured("tiles laid bare on the ellipsoid");

  if (Audits) { (void)engine.inspect(); }
  shot.SettledOver = static_cast<double>(wanted);
  shot.PosedAtS = measured("and the instant it is posed at");

  std::error_code failed;
  const std::string into = std::string("build/shots/") + std::string(under);
  std::filesystem::create_directories(into, failed);
  const std::string writing = into + "/" + std::string(name) + ".writing";
  if (engine.renderer().saveScreenshot(writing).has_value()) {
    std::string bytes;
    if (std::FILE *const held = std::fopen(writing.c_str(), "rb")) {
      char block[65536];
      std::size_t read = 0;
      while ((read = std::fread(block, 1, sizeof block, held)) > 0) { bytes.append(block, read); }
      std::fclose(held);
    }
    shot.Digest = Sha256Hex(bytes).substr(0, 8);
    shot.Wrote = into + "/" + std::string(name) + "-" + shot.Digest + ".png";
    std::filesystem::rename(writing, shot.Wrote, failed);
    shot.Kept = !failed;
  }

  {
    std::vector<std::uint8_t> pixels;
    if (engine.renderer().readPixels(pixels).has_value()) {
      shot.VariationAlongRows = VariationAlongRows(pixels, kWidePx, kHighPx);
    }
  }

  std::vector<double> heldMs;
  heldMs.reserve(static_cast<std::size_t>(kTimedFrames));
  for (int at = 0; at < kTimedFrames; ++at) {
    const int step = at * kWalkViews / kTimedFrames;
    (void)engine.setView("walk" + std::to_string(step));
    const auto before = std::chrono::steady_clock::now();
    if (!engine.advance() || !engine.renderer().render(Extent{})) { break; }
    heldMs.push_back(
        std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - before)
            .count());
    if (heldMs.back() > heldMs[shot.WorstAt]) { shot.WorstAt = heldMs.size() - 1; }
    shot.OverBudget += heldMs.back() > kFrameBudgetMs ? 1u : 0u;
  }
  shot.Frames = heldMs.size();
  std::ranges::sort(heldMs);
  const auto quantile = [&heldMs](double share) {
    if (heldMs.empty()) { return 0.0; }
    const auto which =
        static_cast<std::size_t>(std::llround(share * static_cast<double>(heldMs.size() - 1)));
    return heldMs[which < heldMs.size() ? which : heldMs.size() - 1];
  };
  shot.P50Ms = quantile(0.50);
  shot.P95Ms = quantile(0.95);
  shot.P99Ms = quantile(0.99);

  shot.Measures = engine.measures();

  return shot;
}

} // namespace outshine::Shots
