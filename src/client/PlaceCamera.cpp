#include "Units.h"
#include "PlaceCamera.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <print>
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

constexpr std::uint8_t kByteMost = 255;
constexpr double kFillShare = 0.6;
constexpr double kOverheadPitchDeg = -90.0;
constexpr double kProgressEveryS = 0.25;
constexpr double kBroadQuantile = 0.95;
constexpr double kWidestQuantile = 0.99;

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

LongitudeLatitude WalkedTo(LongitudeLatitude from, double bearingDeg, double alongM) {
  constexpr double kMetresPerDegree = 111320.0;
  const double heading = bearingDeg * kDeg2Rad;
  const double shrink = std::cos(from.LatitudeDeg * kDeg2Rad);
  return {.LongitudeDeg =
              from.LongitudeDeg + alongM * std::sin(heading) /
                                      (kMetresPerDegree * (shrink > kLeastRunM ? shrink : 1.0)),
          .LatitudeDeg = from.LatitudeDeg + alongM * std::cos(heading) / kMetresPerDegree};
}

constexpr std::array<Place, 9> kPlaces{{
    {.Name = "OldTown",
     .LatitudeDeg = 49.3777,
     .LongitudeDeg = 10.179,
     .BearingDeg = 70.0,
     .From = Place::Seen::Eye,
     .SpanM = 0.0,
     .WhenUtc = "2026-06-21T11:19:00Z"},
    {.Name = "Heidelberg",
     .LatitudeDeg = 49.4147,
     .LongitudeDeg = 8.6968,
     .BearingDeg = 108.50,
     .From = Place::Seen::Eye,
     .SpanM = 0.0,
     .WhenUtc = "2026-06-21T11:25:00Z"},
    {.Name = "Shibuya",
     .LatitudeDeg = 35.6595,
     .LongitudeDeg = 139.7005,
     .BearingDeg = 40.0,
     .From = Place::Seen::Eye,
     .SpanM = 0.0,
     .WhenUtc = "2026-06-21T02:41:00Z"},
    {.Name = "CentralPark",
     .LatitudeDeg = 40.7968,
     .LongitudeDeg = -73.9520,
     .BearingDeg = 218.32,
     .From = Place::Seen::Eye,
     .SpanM = 0.0,
     .WhenUtc = "2026-06-21T16:56:00Z"},
    {.Name = "Venice",
     .LatitudeDeg = 45.438,
     .LongitudeDeg = 12.3358,
     .BearingDeg = 30.0,
     .From = Place::Seen::Eye,
     .SpanM = 0.0,
     .WhenUtc = "2026-06-21T11:11:00Z"},
    {.Name = "Jura",
     .LatitudeDeg = 47.2492,
     .LongitudeDeg = 7.5108,
     .BearingDeg = 156.53,
     .From = Place::Seen::Eye,
     .SpanM = 0.0,
     .WhenUtc = "2026-06-21T11:30:00Z"},
    {.Name = "ZurichPlan",
     .LatitudeDeg = 47.3667,
     .LongitudeDeg = 8.5500,
     .BearingDeg = 0.0,
     .From = Place::Seen::Plan,
     .SpanM = 3000.0,
     .WhenUtc = "2026-06-21T11:26:00Z"},
    {.Name = "Kaiserberg",
     .LatitudeDeg = 51.4400,
     .LongitudeDeg = 6.8040,
     .BearingDeg = 45.0,
     .From = Place::Seen::Eye,
     .SpanM = 0.0,
     .WhenUtc = "2026-06-21T11:32:00Z"},
    {.Name = "Koehlbrand",
     .LatitudeDeg = 53.5195,
     .LongitudeDeg = 9.9205,
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
      static_cast<std::size_t>(kWidePx) * static_cast<std::size_t>(kHighPx) * 4u, kByteMost);
  for (int y = 0; y < kHighPx; ++y) {
    const auto shade = static_cast<std::uint8_t>(255 * y / (kHighPx - 1));
    for (int x = 0; x < kWidePx; ++x) {
      const std::size_t at = (static_cast<std::size_t>(y) * static_cast<std::size_t>(kWidePx) +
                              static_cast<std::size_t>(x)) *
                             4u;
      gradient[at] = shade;
      gradient[at + 1] = shade;
      gradient[at + 2] = static_cast<std::uint8_t>(kByteMost - shade);
    }
  }
  return VariationAlongRows(gradient, kWidePx, kHighPx);
}

LogSink *Telling = nullptr;
bool Audits = false;

Scenario::Document ScenarioFor(const Place &place) {
  Scenario::Document stands;
  stands.Ground.Declared = true;
  stands.Ground.Origin.LatitudeDeg = place.LatitudeDeg;
  stands.Ground.Origin.LongitudeDeg = place.LongitudeDeg;
  stands.Ground.PatienceS = 3.0;
  stands.Ground.SightM = kSightM;
  stands.Render.Declared = true;
  stands.Render.Frame = Extent{.WidthPx = kWidePx, .HeightPx = kHighPx};
  stands.Render.Fill = kFillShare;
  stands.Render.Audits = Audits;
  stands.Lit.Declared = true;
  stands.Time.Declared = true;
  stands.Time.Live = false;
  stands.Time.Start = place.WhenUtc;

  Scenario::View watches;
  watches.Id = "station";
  watches.Person = "first";
  watches.Sees.Stands.GlobeAnchor = true;
  watches.Sees.Stands.Geodetic.LatitudeDeg = place.LatitudeDeg;
  watches.Sees.Stands.Geodetic.LongitudeDeg = place.LongitudeDeg;
  const bool overhead = place.From == Place::Seen::Plan;
  watches.Sees.Stands.Geodetic.HeightM = overhead ? kPlanAboveM : kEyeAglM;
  watches.Sees.Stands.SamplesHeight = !overhead;
  watches.Sees.Stands.BearingDeg = place.BearingDeg;
  watches.Sees.Stands.PitchDeg = overhead ? kOverheadPitchDeg : kPitchDeg;
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
    const LongitudeLatitude walked =
        WalkedTo({.LongitudeDeg = place.LongitudeDeg, .LatitudeDeg = place.LatitudeDeg},
                 place.BearingDeg,
                 static_cast<double>(step) * kWalkStepM);
    along.Sees.Stands.Geodetic.LatitudeDeg = walked.LatitudeDeg;
    along.Sees.Stands.Geodetic.LongitudeDeg = walked.LongitudeDeg;
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
  if (Telling != nullptr) { outshine::Engine::logsTo(Telling); }
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
    shot.Why = std::string(place.Name) + " was declared and did not assemble: " + engine.error();
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
  const bool ready =
      engine
          .preload(kPatienceS,
                   [&](const Loading &how) {
                     if (!tells) { return; }
                     if (how.ElapsedS - last.ElapsedS < kProgressEveryS && how.share() < 1.0) {
                       return;
                     }
                     last = how;
                     std::print("\r    loading  terrain {}/{}  osm {}/{}  {} in flight  {:.1f} MB  "
                                "{:.0f} Mbit/s  {:.1f} s   ",
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
  if (tells) { std::println(""); }

  const auto stood = std::chrono::steady_clock::now();
  shot.StreamedS = last.ElapsedS;
  shot.Preloaded = ready;
  if (!ready) {
    shot.Why = std::string(name) +
               " did not preload, so nothing measured after this point is "
               "about the declaration: " +
               std::string(engine.error());
    return shot;
  }
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

  if (Audits && !engine.inspect()) {
    shot.Why = std::string(name) + " refused its audit: " + engine.error();
    return shot;
  }
  shot.SettledOver = static_cast<double>(wanted);
  shot.PosedAtS = measured("and the instant it is posed at");

  std::error_code failed;
  const std::string into = std::string("build/shots/") + std::string(under);
  std::filesystem::create_directories(into, failed);
  const std::string writing = into + "/" + std::string(name) + ".writing";
  if (engine.renderer().saveScreenshot(writing).has_value()) {
    std::string bytes;
    if (std::FILE *const held = std::fopen(writing.c_str(), "rb")) {
      std::array<char, 65536> block{};
      std::size_t read = 0;
      while ((read = std::fread(block.data(), 1, block.size(), held)) > 0) {
        bytes.append(block.data(), read);
      }
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
    if (!engine.setView("walk" + std::to_string(step))) {
      shot.Why = std::string(name) + " could not take view walk" + std::to_string(step) + ": " +
                 engine.error();
      return shot;
    }
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
  shot.P95Ms = quantile(kBroadQuantile);
  shot.P99Ms = quantile(kWidestQuantile);

  shot.Measures = engine.measures();

  return shot;
}

} // namespace outshine::Shots
