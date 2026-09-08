#include "math/Units.h"
#include "math/Quantile.h"
#include "PlaceCamera.h"

#include "io/HeapProbe.h"

#include <algorithm>
#include <expected>
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
#include <tuple>
#include <utility>
#include <vector>

#include "Sha256.h"

namespace outshine::Shots {

namespace Says {
constexpr const char *kTimedAdvanceFailed = " failed to advance a measured frame: ";
constexpr const char *kTimedRenderFailed = " failed to render a measured frame: ";
constexpr const char *kMissingTimingSamples = " has no complete frame timing sample";
}

constexpr std::uint8_t kByteMost = 255;
constexpr double kFillShare = 0.6;
constexpr double kProgressEveryS = 0.25;

namespace {

constexpr double kPatienceS = 15.0;
constexpr double kSightM = 240000.0;

constexpr double kClearDayHaze = 0.0;
constexpr int kTimedFrames = 120;

constexpr std::array<Place, 9> kPlaces{{
    {.Name = "DarmstadtWest",
     .LatitudeDeg = 49.875871,
     .LongitudeDeg = 8.662596,
     .HeightAslM = 205.0,
     .BearingDeg = 252.0,
     .PitchDeg = 2.6,
     .FovDeg = 38.04,
     .WhenUtc = "2026-09-07T10:40:00Z"},
    {.Name = "Wien",
     .LatitudeDeg = 48.233362,
     .LongitudeDeg = 16.411041,
     .HeightAslM = 250.0,
     .BearingDeg = 210.0,
     .PitchDeg = 6.8,
     .FovDeg = 38.04,
     .WhenUtc = "2026-09-07T10:40:00Z"},
    {.Name = "Rosenheim",
     .LatitudeDeg = 47.860299,
     .LongitudeDeg = 12.131823,
     .HeightAslM = 492.0,
     .BearingDeg = 185.0,
     .PitchDeg = -1.6,
     .FovDeg = 30.68,
     .WhenUtc = "2026-09-07T10:40:00Z"},
    {.Name = "Husum",
     .LatitudeDeg = 54.474171,
     .LongitudeDeg = 9.045982,
     .HeightAslM = 20.0,
     .BearingDeg = 35.0,
     .PitchDeg = -7.0,
     .FovDeg = 38.04,
     .WhenUtc = "2026-09-07T10:30:00Z"},
    {.Name = "Olympiaturm",
     .LatitudeDeg = 48.174353,
     .LongitudeDeg = 11.552966,
     .HeightAslM = 700.0,
     .BearingDeg = 310.0,
     .PitchDeg = -6.3,
     .FovDeg = 13.06,
     .WhenUtc = "2026-09-07T10:40:00Z"},
    {.Name = "Graz",
     .LatitudeDeg = 47.079697,
     .LongitudeDeg = 15.412366,
     .HeightAslM = 390.0,
     .BearingDeg = 120.0,
     .PitchDeg = 2.2,
     .FovDeg = 26.23,
     .WhenUtc = "2026-09-07T10:40:00Z"},
    {.Name = "Koerbersee",
     .LatitudeDeg = 47.256121,
     .LongitudeDeg = 10.115857,
     .HeightAslM = 1772.0,
     .BearingDeg = 240.0,
     .PitchDeg = -0.5,
     .FovDeg = 33.97,
     .WhenUtc = "2026-09-07T10:40:00Z"},
    {.Name = "Malcesine",
     .LatitudeDeg = 45.744855,
     .LongitudeDeg = 10.800445,
     .HeightAslM = 140.0,
     .BearingDeg = 290.0,
     .PitchDeg = -2.0,
     .FovDeg = 38.04,
     .WhenUtc = "2026-09-07T10:40:00Z"},
    {.Name = "Feldkirch",
     .LatitudeDeg = 47.232575,
     .LongitudeDeg = 9.598371,
     .HeightAslM = 614.0,
     .BearingDeg = 1.0,
     .PitchDeg = -11.5,
     .FovDeg = 80.72,
     .WhenUtc = "2026-09-07T10:40:00Z"},
}};

}

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
  stands.Ground.Sky.Haze = kClearDayHaze;
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
  watches.Sees.Stands.Geodetic.HeightM = place.HeightAslM;
  watches.Sees.Stands.SamplesHeight = false;
  watches.Sees.Stands.BearingDeg = place.BearingDeg;
  watches.Sees.Stands.PitchDeg = place.PitchDeg;
  watches.Sees.FovDeg = place.FovDeg;
  stands.Views.push_back(watches);
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
  HeapProbe::ForgetPeak();
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
  (void)HeapProbe::Sample();

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
      const Extent frame = engine.swapChain().extent();
      shot.VariationAlongRows = VariationAlongRows(pixels, frame.WidthPx, frame.HeightPx);
    }
  }

  std::vector<double> heldMs;
  std::vector<double> advancedMs;
  std::vector<double> renderedMs;
  heldMs.reserve(static_cast<std::size_t>(kTimedFrames));
  advancedMs.reserve(static_cast<std::size_t>(kTimedFrames));
  renderedMs.reserve(static_cast<std::size_t>(kTimedFrames));
  for (int at = 0; at < kTimedFrames; ++at) {
    const auto before = std::chrono::steady_clock::now();
    if (const auto result = engine.advance(); !result) {
      shot.Why = std::string(name) + Says::kTimedAdvanceFailed + result.error();
      return shot;
    }
    const auto advanced = std::chrono::steady_clock::now();
    if (const auto result = engine.renderer().render(Extent{}); !result) {
      shot.Why = std::string(name) + Says::kTimedRenderFailed + result.error();
      return shot;
    }
    const auto rendered = std::chrono::steady_clock::now();
    (void)HeapProbe::Sample();
    advancedMs.push_back(std::chrono::duration<double, std::milli>(advanced - before).count());
    renderedMs.push_back(std::chrono::duration<double, std::milli>(rendered - advanced).count());
    heldMs.push_back(std::chrono::duration<double, std::milli>(rendered - before).count());
    if (heldMs.back() > heldMs[shot.WorstAt]) { shot.WorstAt = heldMs.size() - 1; }
    shot.OverBudget += heldMs.back() > kFrameBudgetMs ? 1u : 0u;
  }
  shot.Frames = heldMs.size();
  shot.PeakHeapMB = static_cast<double>(HeapProbe::PeakLiveBytes()) / (1024.0 * 1024.0);
  shot.PeakCostMs = HeapProbe::SampleCostMs();
  std::ranges::sort(heldMs);
  const auto p50 = QuantileOf(heldMs, kMiddleQuantile);
  const auto p95 = QuantileOf(heldMs, kBroadQuantile);
  const auto p99 = QuantileOf(heldMs, kWidestQuantile);
  const auto widest =
      [](std::vector<double> &of) -> std::expected<std::pair<double, double>, QuantileError> {
    std::ranges::sort(of);
    const auto rank = QuantileOf(of, kWidestQuantile);
    if (!rank) { return std::unexpected(rank.error()); }
    return std::pair{*rank, of.back()};
  };
  const auto advanced = widest(advancedMs);
  const auto rendered = widest(renderedMs);
  if (!p50 || !p95 || !p99 || !advanced || !rendered) {
    shot.Why = std::string(name) + Says::kMissingTimingSamples;
    return shot;
  }
  shot.P50Ms = *p50;
  shot.P95Ms = *p95;
  shot.P99Ms = *p99;
  std::tie(shot.AdvanceP99Ms, shot.AdvanceWorstMs) = *advanced;
  std::tie(shot.RenderP99Ms, shot.RenderWorstMs) = *rendered;

  shot.Measures = engine.measures();

  return shot;
}

}
