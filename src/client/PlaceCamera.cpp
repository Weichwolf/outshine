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
constexpr const char *kCatalogTooLarge = ": place directory exceeds 4096 entries";
constexpr const char *kNotFile = ": scenario is not a regular file";
constexpr const char *kScenarioSize = ": scenario must contain 1 byte to 1 MiB";
constexpr const char *kEmptyCatalog = ": no .scenario files";
constexpr const char *kPlaceDeclaration =
    ": a place requires world, positive frame, fixed clock and one view";
constexpr const char *kInvalidCamera = ": invalid geodetic camera or projection";
constexpr const char *kInvalidName =
    ": place file stem must use ASCII letters, digits, underscore or hyphen";
constexpr const char *kTimedAdvanceFailed = " failed to advance a measured frame: ";
constexpr const char *kTimedRenderFailed = " failed to render a measured frame: ";
constexpr const char *kMissingTimingSamples = " has no complete frame timing sample";
}

constexpr std::uint8_t kByteMost = 255;
constexpr double kProgressEveryS = 0.25;

namespace {

constexpr double kPatienceS = 15.0;
constexpr int kTimedFrames = 120;

}

namespace {
[[nodiscard]] std::expected<std::vector<std::filesystem::path>, std::string>
PlaceFiles(const std::filesystem::path &directory) {
  std::error_code failed;
  std::filesystem::directory_iterator entry(directory, failed);
  if (failed) { return std::unexpected(directory.string() + ": " + failed.message()); }
  std::vector<std::filesystem::path> paths;
  constexpr std::size_t kMostEntries = 4096;
  constexpr std::uintmax_t kMostScenarioBytes = 1024ULL * 1024ULL;
  std::size_t visited = 0;
  const std::filesystem::directory_iterator end;
  while (entry != end) {
    if (++visited > kMostEntries) {
      return std::unexpected(directory.string() + Says::kCatalogTooLarge);
    }
    if (entry->path().extension() == ".scenario") {
      const bool regular = entry->is_regular_file(failed);
      if (failed) { return std::unexpected(entry->path().string() + ": " + failed.message()); }
      if (!regular) { return std::unexpected(entry->path().string() + Says::kNotFile); }
      const auto bytes = entry->file_size(failed);
      if (failed) { return std::unexpected(entry->path().string() + ": " + failed.message()); }
      if (bytes == 0 || bytes > kMostScenarioBytes) {
        return std::unexpected(entry->path().string() + Says::kScenarioSize);
      }
      paths.push_back(entry->path());
    }
    entry.increment(failed);
    if (failed) { return std::unexpected(directory.string() + ": " + failed.message()); }
  }
  if (paths.empty()) { return std::unexpected(directory.string() + Says::kEmptyCatalog); }
  std::ranges::sort(paths);
  return paths;
}

[[nodiscard]] std::expected<Place, std::string> ReadPlace(const std::filesystem::path &path) {
  Engine reader;
  if (const auto read = reader.readScenario(path.string()); !read) {
    return std::unexpected(path.string() + ": " + read.error());
  }
  const auto &declared = reader.declaration();
  if (!declared.Ground.Declared || !declared.Render.Declared ||
      declared.Render.Frame.WidthPx <= 0 || declared.Render.Frame.HeightPx <= 0 ||
      !declared.Time.Declared || declared.Time.Live || declared.Time.Start.empty() ||
      declared.Views.size() != 1) {
    return std::unexpected(path.string() + Says::kPlaceDeclaration);
  }
  const auto &camera = declared.Views.front().Sees;
  const auto &standing = declared.Views.front().Geographic;
  const auto validPosition = [](const auto &position) {
    return std::isfinite(position.LatitudeDeg) &&
           std::abs(position.LatitudeDeg) <= kDegPerHalfTurn / 2 &&
           std::isfinite(position.LongitudeDeg) &&
           std::abs(position.LongitudeDeg) <= kDegPerHalfTurn;
  };
  auto projection = camera;
  if (!projection.Orthographic) {
    if (projection.NearM == 0) { projection.NearM = Camera::kNearestM; }
    if (projection.FovDeg == 0) { projection.FovDeg = Scenario::kFovUnsaidDeg; }
  }
  Mat4 matrix;
  const double aspect =
      static_cast<double>(declared.Render.Frame.WidthPx) / declared.Render.Frame.HeightPx;
  if (declared.Views.front().Placement != Scenario::CameraPlacement::Geodetic ||
      standing.SamplesHeight || !validPosition(standing.Geodetic) ||
      !validPosition(declared.Ground.Origin) || !std::isfinite(standing.Geodetic.HeightM) ||
      !std::isfinite(standing.BearingDeg) || !std::isfinite(standing.PitchDeg) ||
      std::abs(standing.PitchDeg) > kDegPerHalfTurn / 2 ||
      !projection.projectionMatrix(aspect, matrix)) {
    return std::unexpected(path.string() + Says::kInvalidCamera);
  }
  const std::string name = path.stem().string();
  if (name.empty() ||
      name.find_first_not_of("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_-") !=
          std::string::npos) {
    return std::unexpected(path.string() + Says::kInvalidName);
  }
  return Place{.Name = name, .Declaration = declared};
}

}

std::expected<std::vector<Place>, std::string> LoadPlaces(const std::filesystem::path &directory) {
  const auto paths = PlaceFiles(directory);
  if (!paths) { return std::unexpected(paths.error()); }
  std::vector<Place> places;
  places.reserve(paths->size());
  for (const auto &path : *paths) {
    auto place = ReadPlace(path);
    if (!place) { return std::unexpected(place.error()); }
    places.push_back(std::move(*place));
  }
  return places;
}

const Place *PlaceNamed(std::span<const Place> places, std::string_view name) {
  for (const Place &one : places) {
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
  if (!engine.drawsInto(place.Declaration.Render.Frame)) {
    shot.Why = "the device stood no canvas";
    return shot;
  }

  Scenario::Document stands = place.Declaration;
  stands.Render.Audits = Audits;

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
