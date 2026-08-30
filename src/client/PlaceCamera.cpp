#include "PlaceCamera.h"

#include <algorithm>
#include <cstdlib>
#include <ratio>
#include <system_error>
#include <array>
#include <chrono>
#include <cstdio>
#include <filesystem>

#include <SDL3/SDL.h>

#include <Outshine.h>
#include <Scenario.h>

#include "Sha256.h"

namespace outshine::Shots {
namespace {

constexpr double kPatienceS = 15.0;
constexpr double kSightM = 240000.0;
constexpr double kEyeAglM = 60.0;
constexpr double kPitchDeg = -6.0;
constexpr double kFovDeg = 55.0;
constexpr double kSunElevationDeg = 60.0;
constexpr double kSunBearingDeg = 180.0;
constexpr double kKeyLux = 40000.0;
constexpr int kTimedFrames = 120;

constexpr std::array<Place, 6> kPlaces{{
    {"OldTown", 49.3777, 10.179, 70.0},
    {"Heidelberg", 49.4147, 8.6968, 108.50},
    {"Shibuya", 35.6595, 139.7005, 40.0},
    {"CentralPark", 40.7968, -73.9520, 218.32},
    {"Venice", 45.438, 12.3358, 30.0},
    {"Jura", 47.2492, 7.5108, 156.53},
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
  if (wide < 2 || high < 1 || rgba.size() < (std::size_t)wide * (std::size_t)high * 4u) {
    return 0.0;
  }
  double sum = 0.0;
  std::size_t steps = 0;
  for (int y = 0; y < high; ++y) {
    for (int x = 1; x < wide; ++x) {
      const std::size_t at = ((std::size_t)y * (std::size_t)wide + (std::size_t)x) * 4u;
      for (int channel = 0; channel < 3; ++channel) {
        const int here = rgba[at + (std::size_t)channel];
        const int left = rgba[at - 4u + (std::size_t)channel];
        sum += here > left ? here - left : left - here;
        ++steps;
      }
    }
  }
  return steps > 0 ? sum / (double)steps : 0.0;
}

double ControlVariation() {
  std::vector<std::uint8_t> gradient((std::size_t)kWidePx * (std::size_t)kHighPx * 4u, 255u);
  for (int y = 0; y < kHighPx; ++y) {
    const auto shade = (std::uint8_t)(255 * y / (kHighPx - 1));
    for (int x = 0; x < kWidePx; ++x) {
      const std::size_t at = ((std::size_t)y * (std::size_t)kWidePx + (std::size_t)x) * 4u;
      gradient[at] = shade;
      gradient[at + 1] = shade;
      gradient[at + 2] = (std::uint8_t)(255 - shade);
    }
  }
  return VariationAlongRows(gradient, kWidePx, kHighPx);
}

LogSink *Telling = nullptr;
bool Audits = false;

Shot Take(const Place &place, bool tells) {
  Shot shot;
  if (!SDL_Init(SDL_INIT_VIDEO)) {
    shot.Why = "SDL did not start, so nothing can be drawn";
    return shot;
  }
  Engine engine;
  if (Telling != nullptr) { engine.logsTo(Telling); }
  engine.setRoots(Roots{"src/assets/drive", "src/assets", "/tmp/outshine-drive-cache", false});
  if (!engine.drawsInto(Extent{.WidthPx = kWidePx, .HeightPx = kHighPx})) {
    shot.Why = "the device stood no canvas";
    return shot;
  }

  Scenario stands;
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
  stands.Lit.Key.Lux = kKeyLux;
  stands.Lit.Key.ElevationDeg = kSunElevationDeg;
  stands.Lit.Key.BearingDeg = kSunBearingDeg;

  View watches;
  watches.Id = "station";
  watches.Person = "first";
  watches.Sees.Stands.GlobeAnchor = true;
  watches.Sees.Stands.Geodetic.LatitudeDeg = place.LatDeg;
  watches.Sees.Stands.Geodetic.LongitudeDeg = place.LonDeg;
  watches.Sees.Stands.Geodetic.HeightM = kEyeAglM;
  watches.Sees.Stands.SamplesHeight = true;
  watches.Sees.Stands.BearingDeg = place.BearingDeg;
  watches.Sees.Stands.PitchDeg = kPitchDeg;
  watches.Sees.FovDeg = kFovDeg;
  stands.Views.push_back(watches);

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

  shot.SettledOver = (double)wanted;
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

  std::vector<double> heldMs;
  heldMs.reserve((std::size_t)kTimedFrames);
  for (int at = 0; at < kTimedFrames; ++at) {
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
    const auto which = (std::size_t)std::llround(share * (double)(heldMs.size() - 1));
    return heldMs[which < heldMs.size() ? which : heldMs.size() - 1];
  };
  shot.P50Ms = quantile(0.50);
  shot.P95Ms = quantile(0.95);
  shot.P99Ms = quantile(0.99);

  shot.Measures = engine.measures();

  std::vector<std::uint8_t> pixels;
  if (engine.renderer().readPixels(pixels).has_value()) {
    shot.VariationAlongRows = VariationAlongRows(pixels, kWidePx, kHighPx);
  }
  return shot;
}

} // namespace outshine::Shots
