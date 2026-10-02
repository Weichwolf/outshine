#include "math/Units.h"
#include "math/Quantile.h"
#include "PlaceCamera.h"
#include "PlaceTurn.h"
#include "FramePacer.h"
#include "FrameSchedule.h"
#include "WorldSourcePolicy.h"
#include "SourceCache.h"
#include "PlaceSourcePreparation.h"

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
    ": a place requires world, positive frame and fixed clock";
constexpr const char *kPlaceView = ": a place requires exactly one geodetic view";
constexpr const char *kInvalidCamera = ": invalid geodetic camera or projection";
constexpr const char *kInvalidName =
    ": place file stem must use ASCII letters, digits, underscore or hyphen";
constexpr const char *kTimedAdvanceFailed = " failed to advance a measured frame: ";
constexpr const char *kTimedRenderFailed = " failed to render a measured frame: ";
constexpr const char *kMissingTimingSamples = " has no complete frame timing sample";
}

constexpr std::uint8_t kByteMost = 255;
constexpr double kMillisecondsPerSecond = 1000.0;

namespace {

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

[[nodiscard]] std::expected<size_t, const char *>
PlaceViewIndex(std::span<const Scenario::View> views) {
  size_t selected = views.size();
  for (size_t at = 0; at < views.size(); ++at) {
    if (views[at].Placement != Scenario::CameraPlacement::Geodetic) { continue; }
    if (selected != views.size()) { return std::unexpected(Says::kPlaceView); }
    selected = at;
  }
  if (selected == views.size()) { return std::unexpected(Says::kPlaceView); }
  return selected;
}

[[nodiscard]] std::expected<Place, std::string> ReadPlace(const std::filesystem::path &path) {
  Engine reader;
  if (const auto read = reader.readScenario(path.string()); !read) {
    return std::unexpected(path.string() + ": " + read.error());
  }
  const auto &declared = reader.declaration();
  if (!declared.Ground.Declared || !declared.Render.Declared ||
      declared.Render.Frame.WidthPx <= 0 || declared.Render.Frame.HeightPx <= 0 ||
      !declared.Time.Declared || declared.Time.Live || declared.Time.Start.empty()) {
    return std::unexpected(path.string() + Says::kPlaceDeclaration);
  }
  const auto selected = PlaceViewIndex(declared.Views);
  if (!selected) { return std::unexpected(path.string() + selected.error()); }
  const Scenario::View &view = declared.Views[*selected];
  const auto &camera = view.Sees;
  const auto &standing = view.Geographic;
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
  if (!validPosition(standing.Geodetic) || !validPosition(declared.Ground.Origin) ||
      !std::isfinite(standing.Geodetic.HeightM) || !std::isfinite(standing.BearingDeg) ||
      !std::isfinite(standing.PitchDeg) || std::abs(standing.PitchDeg) > kDegPerHalfTurn / 2 ||
      !projection.projectionMatrix(aspect, matrix)) {
    return std::unexpected(path.string() + Says::kInvalidCamera);
  }
  const std::string name = path.stem().string();
  if (name.empty() ||
      name.find_first_not_of("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_-") !=
          std::string::npos) {
    return std::unexpected(path.string() + Says::kInvalidName);
  }
  Scenario::Document capture = declared;
  capture.Views = {view};
  return Place{.Name = name, .Declaration = std::move(capture)};
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

namespace {
bool OpenPlace(Engine &engine, const Place &place, Shot &shot, Roots roots) {
  if (!SDL_Init(SDL_INIT_VIDEO)) {
    shot.Why = "SDL did not start, so nothing can be drawn";
    return false;
  }
  if (Telling != nullptr) { engine.logsTo(Telling); }
  if (const auto rooted = Client::WithSourceCache(std::move(roots)).and_then([&](Roots resolved) {
        return engine.setRoots(std::move(resolved));
      });
      !rooted) {
    shot.Why = "the engine rejected its roots: " + rooted.error();
    return false;
  }
  if (const auto targeted = engine.setRenderTarget(place.Declaration.Render.Frame); !targeted) {
    shot.Why = "the device stood no canvas: " + targeted.error();
    return false;
  }

  Scenario::Document stands = place.Declaration;
  stands.Views = PlaceTurn(place.Declaration.Views.front(),
                           {.LongitudeDeg = stands.Ground.Origin.LongitudeDeg,
                            .LatitudeDeg = stands.Ground.Origin.LatitudeDeg});
  stands.Render.Audits = Audits;

  const auto began = std::chrono::steady_clock::now();
  if (const auto sources = Client::ConfigureWorldSources(stands); !sources) {
    shot.Why = std::string(place.Name) + " has invalid world sources: " + sources.error();
    return false;
  }
  if (const auto declared = engine.declare(stands); !declared) {
    shot.Why = std::string(place.Name) + " was not declared: " + declared.error();
    return false;
  }
  if (const auto assembled = engine.assemble(); !assembled) {
    shot.Why = std::string(place.Name) + " was not assembled: " + assembled.error();
    return false;
  }
  shot.StandingMs =
      std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - began).count();
  return true;
}
}

std::string Prepare(const Place &place, double patienceS) {
  const auto began = std::chrono::steady_clock::now();
  auto roots = Client::WithSourceCache(
      Roots{.Assets = "src/assets/drive", .Shipped = "src/assets", .Cache = {}});
  if (!roots) { return std::move(roots.error()); }
  auto declared = place.Declaration;
  if (auto configured = Client::ConfigureWorldSources(declared); !configured) {
    return std::move(configured.error());
  }
  if (auto sources = Client::PreparePlaceSources(
          declared,
          *roots,
          patienceS,
          [&](Generators::Osm::SourceCacheProgress how) {
            std::println("PREPARE {} source cache elapsed {:.1f} s; {}/{} validated cells",
                         place.Name,
                         how.ElapsedS,
                         how.ValidatedCells,
                         how.RequiredCells);
          });
      !sources) {
    return std::move(sources.error());
  }
  const double elapsed =
      std::chrono::duration<double>(std::chrono::steady_clock::now() - began).count();
  if (elapsed >= patienceS) { return "place preparation deadline exceeded before world assembly"; }
  Engine engine;
  Shot shot;
  if (!OpenPlace(engine, place, shot, std::move(*roots))) { return shot.Why; }
  double last = 0;
  const double remaining =
      patienceS - std::chrono::duration<double>(std::chrono::steady_clock::now() - began).count();
  if (remaining <= 0) { return "place preparation deadline exceeded during world assembly"; }
  const Result ready = engine.preload(remaining, [&](const Loading &how) {
    if (how.ElapsedS - last < 5) { return; }
    last = how.ElapsedS;
    std::println("PREPARE {} elapsed {:.1f} s", place.Name, how.ElapsedS);
    for (const auto &measure : engine.measures()) {
      if (measure.Name.starts_with("flora: crown")) {
        std::println("        {}: {:.0f}", measure.Name, measure.Value);
      }
    }
  });
  return ready ? std::string{} : ready.error();
}

Shot Take(const Place &place, bool tells, double preloadSeconds, Roots roots) {
  Engine engine;
  Shot shot;
  if (!OpenPlace(engine, place, shot, std::move(roots))) { return shot; }
  Shot drawn =
      Draw(engine, place.Name, tells, "places", preloadSeconds, engine.declaration().Views);
  drawn.StandingMs = shot.StandingMs;
  drawn.LoadingAtEnd = engine.loading();
  drawn.Playable = engine.settled(WorldQuality::Playable);
  drawn.Refined = engine.settled(WorldQuality::Refined);
  return drawn;
}

namespace {
bool PreloadShot(
    Engine &engine, std::string_view name, bool tells, double preloadSeconds, Shot &shot) {
  const auto asked = std::chrono::steady_clock::now();
  if (tells) { std::println("    loading {} to refined world quality", name); }
  const Result ready = engine.preload(preloadSeconds, WorldQuality::Refined);
  const Loading last = engine.loading();
  const auto stood = std::chrono::steady_clock::now();
  shot.StreamedS = last.PreloadMs / kMillisecondsPerSecond;
  shot.Preloaded = ready.has_value();
  shot.LoadingMs = std::chrono::duration<double, std::milli>(stood - asked).count();
  if (!ready) {
    shot.Why = std::string(name) +
               " did not preload, so nothing measured after this point is "
               "about the declaration: " +
               ready.error();
    return false;
  }
  (void)HeapProbe::Sample();

  return true;
}

bool MeasureFrames(Engine &engine,
                   std::string_view name,
                   Shot &shot,
                   std::span<const Scenario::View> turn) {
  const auto count = turn.empty() ? static_cast<std::size_t>(kTimedFrames) : turn.size();
  constexpr double kBytesPerMiB = 1024 * 1024;
  std::vector<double> heldMs;
  std::vector<double> advancedMs;
  std::vector<double> renderedMs;
  heldMs.reserve(count);
  advancedMs.reserve(count);
  renderedMs.reserve(count);
  Client::FramePacer pacer;
  const auto began = std::chrono::steady_clock::now();
  const Client::FrameSchedule schedule(SDL_GetTicksNS());
  for (std::size_t at = 0; at < count; ++at) {
    if (turn.empty()) {
      pacer.Wait();
    } else {
      schedule.Wait(at);
    }
    const auto before = std::chrono::steady_clock::now();
    const auto selected = turn.empty() ? Result{} : engine.setView(turn[(at + 1) % count].Id);
    if (!selected) {
      shot.Why = std::string(name) + " could not select its turn view: " + selected.error();
      return false;
    }
    if (const auto result = engine.advance(); !result) {
      shot.Why = std::string(name) + Says::kTimedAdvanceFailed + result.error();
      return false;
    }
    if (!turn.empty() && !engine.settled(WorldQuality::Refined)) {
      shot.Why = std::string(name) + " lost complete world coverage during its turn: " +
                 engine.unsettledReasons(WorldQuality::Refined);
      return false;
    }
    const auto advanced = std::chrono::steady_clock::now();
    if (const auto result = engine.renderer().render(Extent{}); !result) {
      shot.Why = std::string(name) + Says::kTimedRenderFailed + result.error();
      return false;
    }
    const auto rendered = std::chrono::steady_clock::now();
    (void)HeapProbe::Sample();
    advancedMs.push_back(std::chrono::duration<double, std::milli>(advanced - before).count());
    renderedMs.push_back(std::chrono::duration<double, std::milli>(rendered - advanced).count());
    heldMs.push_back(std::chrono::duration<double, std::milli>(rendered - before).count());
    if (heldMs.back() > heldMs[shot.WorstAt]) { shot.WorstAt = heldMs.size() - 1; }
    shot.OverBudget += heldMs.back() > kFrameBudgetMs ? 1u : 0u;
  }
  shot.MeasurementMs =
      std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - began).count();
  shot.TurnDegrees = turn.empty() ? 0.0 : 2.0 * kDegPerHalfTurn;
  shot.Frames = heldMs.size();
  shot.PeakHeapMB = static_cast<double>(HeapProbe::PeakLiveBytes()) / kBytesPerMiB;
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
    return false;
  }
  shot.P50Ms = *p50;
  shot.P95Ms = *p95;
  shot.P99Ms = *p99;
  std::tie(shot.AdvanceP99Ms, shot.AdvanceWorstMs) = *advanced;
  std::tie(shot.RenderP99Ms, shot.RenderWorstMs) = *rendered;

  return true;
}
}

Shot Draw(Engine &engine,
          std::string_view name,
          bool tells,
          std::string_view under,
          double preloadSeconds,
          std::span<const Scenario::View> turn) {
  Shot shot;
  HeapProbe::ForgetPeak();
  if (!PreloadShot(engine, name, tells, preloadSeconds, shot)) { return shot; }
  if (!MeasureFrames(engine, name, shot, turn)) { return shot; }
  if (engine.declaration().Ground.Declared && !engine.settled(WorldQuality::Refined)) {
    shot.Why = std::string(name) + " did not reach refined world quality after " +
               std::to_string(shot.Frames) +
               " measured frames: " + engine.unsettledReasons(WorldQuality::Refined);
    return shot;
  }
  {
    auto capture = engine.beginCapture();
    if (!capture) {
      shot.Why = std::string(name) + " did not begin capture: " + capture.error();
      return shot;
    }

    const auto measured = [&engine](const char *what) {
      for (const DiagnosticSample &held : engine.measures()) {
        if (held.Name == what) { return held.Value; }
      }
      return 0.0;
    };
    shot.Triangles = measured("building triangles the world meshed");
    shot.BareTiles = measured("tiles laid bare on the ellipsoid");

    if (Audits) {
      if (const auto inspected = engine.inspect(); !inspected) {
        shot.Why = std::string(name) + " refused its audit: " + inspected.error();
        return shot;
      }
    }
    shot.PosedAtS = measured("and the instant it is posed at");

    std::error_code failed;
    const std::string into = std::string("build/shots/") + std::string(under);
    std::filesystem::create_directories(into, failed);
    const std::string writing = into + "/" + std::string(name) + ".writing";
    if (engine.renderer().saveScreenshot(writing)) {
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
  }

  shot.Measures.assign(engine.measures().begin(), engine.measures().end());

  return shot;
}

}
