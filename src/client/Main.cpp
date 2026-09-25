#include "CommandLine.h"
#include "ProcessBoundary.h"
#include "ShotOptions.h"
#include <chrono>
#include <expected>
#include <optional>
#include <cstdio>
#include <print>
#include <ratio>
#include <cstddef>
#include <cstdlib>
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
#include "PixelProbe.h"
#include "RenderAsset.h"
#include "ScenarioRoundTrip.h"
#include "ScenarioCapture.h"
#include "format/Number.h"
#include <cmath>

namespace {

class Telling final : public outshine::LogSink {
public:
  void Write(double simTimeS,
             outshine::LogLevel level,
             Saying who,
             std::span<const outshine::LogField> fields) noexcept override {
    if (level == outshine::LogLevel::Debug && !Loud) { return; }
    try {
      std::print("t={:.1f} {:<5} {:<8} {:<7} {}",
                 simTimeS,
                 Name(level),
                 who.Unit != nullptr ? who.Unit : "",
                 outshine::nameOf(who.Tag),
                 who.Event);
      for (const outshine::LogField &one : fields) { std::print(" {}={}", one.Key, one.Value); }
      std::println("");
    } catch (...) { ReportFailure(); }
  }

  bool Loud = false;

private:
  static void ReportFailure() noexcept { std::fputs("outshine-client: log sink failed\n", stderr); }

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

constexpr double kMillisecondsPerSecond = 1000.0;

void Tell(const Shot &shot, std::string_view name) {
  if (!shot.Why.empty()) {
    std::println("SHOT    {:<26} -- {}", name, shot.Why);
    return;
  }
  std::println("SHOT    {:<26} {}  p50 {:6.2f}  p95 {:6.2f}  p99 {:6.2f} ms  {} of {} over {:.2f}, "
               "worst at {}  [sim p99 {:.2f} worst {:.2f} | draw p99 {:.2f} worst {:.2f}]",
               name,
               shot.Digest.empty() ? "--------" : std::string_view{shot.Digest},
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
  std::println(
      "        {:.0f} triangle(s), {:.0f} bare tile(s), varies by {:.3f} of 255 along its rows; "
      "{:.1f} s stood, {:.1f} s waited ({:.1f} s streamed); peak heap {:.0f} MB; {}",
      shot.Triangles,
      shot.BareTiles,
      shot.VariationAlongRows,
      shot.StandingMs / kMillisecondsPerSecond,
      shot.LoadingMs / kMillisecondsPerSecond,
      shot.StreamedS,
      shot.PeakHeapMB,
      shot.Kept ? std::string_view{shot.Wrote} : "NO PICTURE");
}

void Row(const Shot &shot, std::string_view name) {
  std::println("ROW\t{}\t{}\t{}\t{:.4f}\t{:.4f}\t{:.4f}\t{}\t{}\t{}\t{:.0f}\t{:.0f}\t{:.4f}\t{}\t{:"
               ".0f}\t{:.4f}\t{}",
               name,
               shot.Digest.empty() ? "-" : std::string_view{shot.Digest},
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
               shot.Why.empty() ? "-" : std::string_view{shot.Why});
}

void Usage(std::string_view verb = {}) {
  if (verb.empty()) {
    std::println("Usage: outshine-client [--places <directory>] <command> [arguments]\n"
                 "  render      capture glTF/GLB to PNG\n"
                 "  run         capture a scenario or route motion\n"
                 "  shots       capture named places\n"
                 "  measures    capture a scenario and print engine diagnostics\n"
                 "  prepare     preload a named place\n"
                 "  places      list place cameras\n"
                 "  roundtrip   check scenario serialization\n"
                 "  height      query terrain elevation\n\n"
                 "Use <command> --help for options. --places defaults to src/assets/places.\n"
                 "--stats on render, run, shots or measures prints "
                 "STAT<TAB>name<TAB>key<TAB>value<TAB>unit rows.");
    return;
  }
  if (verb == "render") {
    std::println("Usage: outshine-client render <asset.gltf|asset.glb> <width>x<height> "
                 "<output.png> [options]\n"
                 "  --camera auto|index             default: first unambiguous camera, else auto\n"
                 "  --time <seconds>                animation time, default 0\n"
                 "  --animation <index>             select animation clip\n"
                 "  --variant <name>                select material variant\n"
                 "  --position x,y,z --look-at x,y,z  camera in local metres; supply both\n"
                 "  --fov <degrees>                 vertical field of view\n"
                 "  --lighting auto|authored|studio  default auto\n"
                 "  --exposure <multiplier>         positive linear multiplier, default 1\n"
                 "  --stats                         stage timings, frame count and resolution\n"
                 "Resolution axes: 1..4096 px. Output directory must exist.");
  } else if (verb == "run" || verb == "measures") {
    std::println(
        "Usage: outshine-client {} [options] <scenario> [name]\n"
        "  --view <id>                     capture this declared camera\n"
        "  --at-seconds <s>                nonnegative scenario time; requires --view\n"
        "  --motion                        render every paced tick; requires view and time\n"
        "  --samples                       save route-decile PNGs; requires --motion\n"
        "  --quality playable|refined      minimum final capture quality (default playable)\n"
        "  --into <folder>                 relative to build/shots/ (default khronos)\n"
        "  --cache-dir <directory>         default /tmp/outshine-drive-cache\n"
        "  --offline                       use only cached and shipped sources\n"
        "  --rows                          machine-readable capture rows\n"
        "  --stats                         timing, readiness and source STAT rows\n"
        "  --probe-pixel <x,y>             inspect a final pixel; requires --quality refined\n"
        "  PIXEL row: name, x, y, RGBA8, linear RGB, device depth, normal XYZ, surface ID, "
        "quality.\n"
        "  measures additionally prints engine diagnostics and visible structure source keys.\n"
        "Motion writes a per-frame TSV with time, station, camera position, advance/render "
        "time, readiness, ground candidate progress, previous-frame GPU diagnostics and "
        "left/centre/right road contact with eye clearance.",
        verb);
  } else if (verb == "shots") {
    std::println("Usage: outshine-client shots [options] [--all | <place> ...]\n"
                 "  --preload-seconds <seconds>     positive preparation timeout, default 15\n"
                 "  --cache-dir <directory>         default /tmp/outshine-drive-cache\n"
                 "  --offline                       use only cached and shipped sources\n"
                 "  --no-vegetation                 disable vegetation in captured scenarios\n"
                 "  --audit                         run image audit\n"
                 "  --measures                      print engine diagnostic samples\n"
                 "  --rows                          machine-readable capture rows\n"
                 "  --stats                         timing, readiness and source STAT rows\n"
                 "With no place, capture all. Preparation time is separate from frame timing.");
  } else if (verb == "prepare") {
    std::println(
        "Usage: outshine-client prepare <place> <timeout-seconds>\n"
        "Preload a named place with a positive timeout; report readiness, not frame rate.");
  } else if (verb == "places") {
    std::println("Usage: outshine-client [--places <directory>] places\n"
                 "List each place camera, geodetic pose, resolution and time as TSV.");
  } else if (verb == "roundtrip") {
    std::println("Usage: outshine-client [--places <directory>] roundtrip\n"
                 "Write, read and rewrite every place scenario.");
  } else if (verb == "height") {
    std::println("Usage: outshine-client height <latitude-deg> <longitude-deg>\n"
                 "Query terrain elevation in metres; latitude [-90,90], longitude [-180,180].");
  }
  if (verb == "render" || verb == "run" || verb == "measures" || verb == "shots") {
    std::println("STAT TSV: STAT<TAB>name<TAB>key<TAB>value<TAB>unit; "
                 "status is ok or failed. Timings are milliseconds.\n"
                 "run: setup_ms; run/shots: preload/wait timings, playable/refined, tile and "
                 "source counts.\n"
                 "render: elapsed/prepare/assemble/draw/save_ms, draw_frames, width/height_px.\n"
                 "route motion: frame count, p50/p95/p99, budget/readiness, contact gaps, "
                 "eye clearance and peak heap.");
  }
  if (verb == "run" || verb == "measures") {
    std::println("PIXEL TSV: PIXEL<TAB>name<TAB>x<TAB>y<TAB>RGBA8<TAB>"
                 "linear-RGB<TAB>device-depth<TAB>normal-XYZ<TAB>surface-ID<TAB>quality; "
                 "multi-component fields are comma-separated.");
  }
}

[[nodiscard]] std::optional<std::string_view>
RequestedHelp(const outshine::Client::CommandLine &command) {
  if (command.Verb == "help" || command.Verb == "--help") { return std::string_view{}; }
  if (command.Arguments.size() != 1 || std::string_view(command.Arguments.front()) != "--help") {
    return std::nullopt;
  }
  const std::string_view verb = command.Verb;
  if (verb == "render" || verb == "run" || verb == "shots" || verb == "measures" ||
      verb == "prepare" || verb == "places" || verb == "roundtrip" || verb == "height") {
    return verb;
  }
  return std::nullopt;
}

void PrintStats(std::string_view name,
                const outshine::Loading &loading,
                double elapsedMs,
                bool succeeded,
                bool playable,
                bool refined,
                std::optional<double> setupMs = std::nullopt) {
  const auto row = [name](std::string_view key, auto value, std::string_view unit) {
    std::println("STAT\t{}\t{}\t{}\t{}", name, key, value, unit);
  };
  row("status", succeeded ? "ok" : "failed", "-");
  row("elapsed_ms", elapsedMs, "ms");
  if (setupMs) { row("setup_ms", *setupMs, "ms"); }
  row("preload_ms", loading.PreloadMs, "ms");
  row("preload_pump_ms", loading.PreloadPumpMs, "ms");
  row("preload_flush_ms", loading.PreloadFlushMs, "ms");
  row("preload_await_ms", loading.PreloadAwaitMs, "ms");
  row("preload_pumps", loading.PreloadPumps, "calls");
  row("preload_flushes", loading.PreloadFlushes, "calls");
  row("preload_awaits", loading.PreloadAwaits, "calls");
  row("wait_structure_ms", loading.Waited.StructureMs, "ms");
  row("wait_class_ms", loading.Waited.ClassMs, "ms");
  row("wait_tile_ms", loading.Waited.TileMs, "ms");
  row("wait_world_worker_ms", loading.Waited.WorldWorkerMs, "ms");
  row("wait_idle_ms", loading.Waited.IdleMs, "ms");
  row("wait_structure_calls", loading.Waited.StructureCalls, "calls");
  row("wait_class_calls", loading.Waited.ClassCalls, "calls");
  row("wait_tile_calls", loading.Waited.TileCalls, "calls");
  row("wait_world_worker_calls", loading.Waited.WorldWorkerCalls, "calls");
  row("wait_idle_calls", loading.Waited.IdleCalls, "calls");
  row("wait_structure_signals", loading.Waited.StructureSignals, "signals");
  row("wait_class_signals", loading.Waited.ClassSignals, "signals");
  row("wait_tile_signals", loading.Waited.TileSignals, "signals");
  row("wait_world_worker_signals", loading.Waited.WorldWorkerSignals, "signals");
  row("wait_tile_no_outstanding_ms", loading.Waited.TileNoOutstandingMs, "ms");
  row("wait_tile_no_outstanding_calls", loading.Waited.TileNoOutstandingCalls, "calls");
  row("playable", playable ? 1 : 0, "bool");
  row("refined", refined ? 1 : 0, "bool");
  row("ground_arrived", loading.GroundArrived, "tiles");
  row("ground_wanted", loading.GroundWanted, "tiles");
  row("vector_arrived", loading.VectorArrived, "tiles");
  row("vector_wanted", loading.VectorWanted, "tiles");
  row("outstanding", loading.Outstanding, "tiles");
  row("store_hits", loading.StoreHits, "reads");
  row("store_misses", loading.StoreMisses, "reads");
  row("store_writes", loading.StoreWrites, "writes");
  row("provider_starts", loading.ProviderStarts, "calls");
  row("remote_starts", loading.RemoteStarts, "calls");
  row("provider_retries", loading.ProviderRetries, "retries");
  row("source_deliveries", loading.SourceDeliveries, "deliveries");
  row("source_from_store", loading.SourceFromStore, "deliveries");
  row("source_bytes", loading.SourceBytes, "bytes");
}

[[nodiscard]] outshine::Roots ClientRoots(std::string_view cacheDirectory, bool offline) {
  return {.Assets = "src/assets/drive",
          .Shipped = "src/assets",
          .Cache = std::string(cacheDirectory),
          .Offline = offline};
}

[[nodiscard]] bool
Stands(outshine::Engine &engine,
       outshine::Extent frame = {.WidthPx = outshine::Shots::kWidePx,
                                 .HeightPx = outshine::Shots::kHighPx},
       outshine::Roots roots = ClientRoots(outshine::Client::kDefaultCacheDirectory, false)) {
  if (!SDL_Init(SDL_INIT_VIDEO)) {
    std::println("outshine-client: SDL did not start");
    return false;
  }
  engine.logsTo(&gTelling);
  if (const auto rooted = engine.setRoots(std::move(roots)); !rooted) {
    std::println("outshine-client: the engine rejected its roots -- {}", rooted.error());
    return false;
  }
  if (frame.WidthPx > 0 && frame.HeightPx > 0) {
    if (const auto targeted = engine.setRenderTarget(frame); !targeted) {
      std::println("outshine-client: the device stood no canvas -- {}", targeted.error());
      return false;
    }
  }
  return true;
}

void ReportShot(const Shot &shot,
                std::string_view name,
                const outshine::Client::ShotOptions &options,
                std::chrono::steady_clock::time_point began) {
  if (options.Rows) {
    Row(shot, name);
  } else {
    Tell(shot, name);
  }
  if (options.Measures) {
    for (const outshine::DiagnosticSample &measure : shot.Measures) {
      std::println("        {:<56} {:14.3f} {}", measure.Name, measure.Value, measure.Unit);
    }
  }
  if (options.Stats) {
    const double elapsedMs =
        std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - began).count();
    PrintStats(name,
               shot.LoadingAtEnd,
               elapsedMs,
               shot.Why.empty() && shot.Kept,
               shot.Playable,
               shot.Refined);
  }
}

int TakeShots(std::span<const Place> places, int argc, const char *const *argv) {
  std::vector<const Place *> taking;
  const std::span<const char *const> arguments(argv, static_cast<size_t>(argc));
  const auto parsed = outshine::Client::ReadShotOptions(arguments);
  if (!parsed) {
    std::println(stderr, "outshine-client: {}", parsed.error());
    return 2;
  }
  const auto &options = *parsed;
  outshine::Shots::Audits = options.Audit;
  const auto names = arguments.subspan(options.FirstPlace);
  if (options.All || names.empty()) {
    for (const Place &one : places) { taking.push_back(&one); }
  } else {
    for (const char *name : names) {
      const Place *const named = outshine::Shots::PlaceNamed(places, name);
      if (named == nullptr) {
        std::println(stderr, "outshine-client: no place is called '{}'", name);
        return 2;
      }
      taking.push_back(named);
    }
  }
  std::println("CONTROL\t{:.4f}", outshine::Shots::ControlVariation());
  std::println("SCENARIO\tvegetation={}", options.Vegetation ? "yes" : "no");
  std::println("PRELOAD\t{} s", options.PreloadSeconds);
  int refused = 0;
  for (const Place *const one : taking) {
    outshine::Shots::Telling = &gTelling;
    const auto began = std::chrono::steady_clock::now();
    const Shot shot = outshine::Shots::Take(*one,
                                            !options.Rows,
                                            options.Vegetation,
                                            options.PreloadSeconds,
                                            ClientRoots(options.CacheDirectory, options.Offline));
    ReportShot(shot, one->Name, options, began);
    refused += shot.Why.empty() && shot.Kept ? 0 : 1;
  }
  return refused == 0 ? 0 : 1;
}

struct ScenarioRunOptions {
  int Argc = 0;
  const char *const *Argv = nullptr;
  bool Rows = false;
  bool Stats = false;
  bool Offline = false;
  std::string_view CacheDirectory = outshine::Client::kDefaultCacheDirectory;
  std::string Into = "khronos";
  std::string_view SelectedView;
  double AtS = 0.0;
  bool RenderMotion = false;
  bool SampleImages = false;
  bool AwaitRefined = false;
  std::optional<outshine::Client::PixelCoordinate> ProbePixel;
};

[[nodiscard]] std::expected<void, int> ValidateScenarioRunOptions(const ScenarioRunOptions &options,
                                                                  bool hasTime) {
  if (hasTime && options.SelectedView.empty()) {
    std::println(stderr, "outshine-client: --at-seconds requires --view");
    return std::unexpected(2);
  }
  if (options.RenderMotion && (!hasTime || options.SelectedView.empty())) {
    std::println(stderr, "outshine-client: --motion requires --view and --at-seconds");
    return std::unexpected(2);
  }
  if (options.SampleImages && !options.RenderMotion) {
    std::println(stderr, "outshine-client: --samples requires --motion");
    return std::unexpected(2);
  }
  if (options.ProbePixel && (!hasTime || options.SelectedView.empty())) {
    std::println(stderr, "outshine-client: --probe-pixel requires --view and --at-seconds");
    return std::unexpected(2);
  }
  if (options.ProbePixel && !options.AwaitRefined) {
    std::println(stderr, "outshine-client: --probe-pixel requires --quality refined");
    return std::unexpected(2);
  }
  return {};
}

[[nodiscard]] bool ReadRunFlag(std::string_view flag, ScenarioRunOptions &options) {
  if (flag == "--rows") {
    options.Rows = true;
  } else if (flag == "--stats") {
    options.Stats = true;
  } else if (flag == "--offline") {
    options.Offline = true;
  } else if (flag == "--motion") {
    options.RenderMotion = true;
  } else if (flag == "--samples") {
    options.SampleImages = true;
  } else {
    return false;
  }
  return true;
}

[[nodiscard]] std::expected<bool, int> ReadCaptureQuality(const char *value) {
  if (value == nullptr ||
      (std::string_view(value) != "playable" && std::string_view(value) != "refined")) {
    std::println(stderr, "outshine-client: --quality requires playable or refined");
    return std::unexpected(2);
  }
  return std::string_view(value) == "refined";
}

[[nodiscard]] std::expected<outshine::Client::PixelCoordinate, int>
ReadCapturePixel(const char *value) {
  if (value == nullptr) {
    std::println(stderr, "outshine-client: --probe-pixel requires nonnegative integer x,y");
    return std::unexpected(2);
  }
  const auto pixel = outshine::Client::ParsePixelCoordinate(value);
  if (!pixel) {
    std::println(stderr, "outshine-client: {}", pixel.error());
    return std::unexpected(2);
  }
  return *pixel;
}

[[nodiscard]] std::expected<bool, int>
ReadRunValue(std::string_view flag, const char *value, ScenarioRunOptions &options, bool &hasTime) {
  if (flag == "--cache-dir") {
    if (value == nullptr || !outshine::Client::ValidCacheDirectory(value)) {
      std::println(stderr, "outshine-client: --cache-dir requires a nonempty directory");
      return std::unexpected(2);
    }
    options.CacheDirectory = value;
    return true;
  }
  if (flag == "--into" || flag == "--view") {
    if (value == nullptr) {
      std::println(stderr, "outshine-client: {} requires a value", flag);
      return std::unexpected(2);
    }
    if (flag == "--into") {
      options.Into = value;
    } else {
      options.SelectedView = value;
    }
    return true;
  }
  if (flag == "--at-seconds") {
    if (value == nullptr) {
      std::println(stderr, "outshine-client: --at-seconds needs a nonnegative time in seconds");
      return std::unexpected(2);
    }
    const auto parsed = outshine::ParseFiniteNumber(value);
    if (!parsed || *parsed < 0.0) {
      std::println(stderr, "outshine-client: --at-seconds needs a nonnegative time in seconds");
      return std::unexpected(2);
    }
    options.AtS = *parsed;
    hasTime = true;
    return true;
  }
  if (flag == "--quality") {
    const auto quality = ReadCaptureQuality(value);
    if (!quality) { return std::unexpected(quality.error()); }
    options.AwaitRefined = *quality;
    return true;
  }
  if (flag == "--probe-pixel") {
    const auto pixel = ReadCapturePixel(value);
    if (!pixel) { return std::unexpected(pixel.error()); }
    options.ProbePixel = *pixel;
    return true;
  }
  return false;
}

[[nodiscard]] std::expected<ScenarioRunOptions, int>
ParseScenarioRunOptions(int argc, const char *const *argv) {
  ScenarioRunOptions options;
  options.Argc = argc;
  options.Argv = argv;
  bool hasTime = false;
  while (argc > 0 && argv[0][0] == '-') {
    if (ReadRunFlag(argv[0], options)) {
      --argc;
      ++argv;
      continue;
    }
    const auto value = ReadRunValue(argv[0], argc > 1 ? argv[1] : nullptr, options, hasTime);
    if (!value) { return std::unexpected(value.error()); }
    if (!*value) { break; }
    argc -= 2;
    argv += 2;
  }
  if (const auto valid = ValidateScenarioRunOptions(options, hasTime); !valid) {
    return std::unexpected(valid.error());
  }
  options.Argc = argc;
  options.Argv = argv;
  return options;
}

int CaptureView(outshine::Engine &engine,
                std::string_view named,
                const ScenarioRunOptions &options) {
  const auto captured =
      outshine::Client::CaptureScenarioView(engine,
                                            {.View = options.SelectedView,
                                             .Name = named,
                                             .Into = options.Into,
                                             .AtS = options.AtS,
                                             .RenderMotion = options.RenderMotion,
                                             .SampleImages = options.SampleImages,
                                             .AwaitRefined = options.AwaitRefined});
  if (!captured) {
    std::println(stderr, "outshine-client: {}", captured.error());
    return 1;
  }
  if (options.Stats) {
    std::println("STAT\t{}\trequested_quality\t{}\tquality",
                 named,
                 options.AwaitRefined ? "refined" : "playable");
  }
  if (options.ProbePixel) {
    if (const auto probed = outshine::Client::ReportPixel(engine, named, *options.ProbePixel);
        !probed) {
      std::println(stderr, "outshine-client: {}", probed.error());
      return 1;
    }
  }
  if (options.Rows) {
    std::println("CAPTURE\t{}\t{}\t{:.6f}\t{:.3f}\t{:.3f}\t{}",
                 named,
                 options.SelectedView,
                 captured->SimTimeS,
                 captured->RouteStationM,
                 captured->RouteLengthM,
                 captured->Path);
  } else {
    std::println("CAPTURE {} view={} t={:.3f} s station={:.3f}/{:.3f} m {}",
                 named,
                 options.SelectedView,
                 captured->SimTimeS,
                 captured->RouteStationM,
                 captured->RouteLengthM,
                 captured->Path);
  }
  if (!options.RenderMotion) { return 0; }
  if (options.Rows) {
    std::println("MOTION\t{}\t{}\t{}\t{:.3f}\t{:.3f}\t{:.3f}\t{}\t{}\t{:.3f}\t{}",
                 named,
                 options.SelectedView,
                 captured->Frames,
                 captured->P50Ms,
                 captured->P95Ms,
                 captured->P99Ms,
                 captured->OverBudget,
                 captured->Unsettled,
                 captured->PeakHeapMiB,
                 captured->TracePath);
    if (!captured->LastUnsettledReason.empty()) {
      std::println("UNSETTLED\t{}", captured->LastUnsettledReason);
    }
  } else {
    std::println("MOTION {} frames={} p50={:.2f} p95={:.2f} p99={:.2f} ms over={} unsettled={} "
                 "peak={:.1f} MiB {}",
                 named,
                 captured->Frames,
                 captured->P50Ms,
                 captured->P95Ms,
                 captured->P99Ms,
                 captured->OverBudget,
                 captured->Unsettled,
                 captured->PeakHeapMiB,
                 captured->TracePath);
    if (!captured->LastUnsettledReason.empty()) {
      std::println("UNSETTLED {}", captured->LastUnsettledReason);
    }
  }
  if (options.SampleImages) {
    std::println("SAMPLES\t{}\tbuild/shots/{}", captured->SampleImages, options.Into);
  }
  if (options.Stats) {
    std::println("STAT\t{}\tmotion_frames\t{}\tframes", named, captured->Frames);
    std::println("STAT\t{}\tmotion_p50_ms\t{}\tms", named, captured->P50Ms);
    std::println("STAT\t{}\tmotion_p95_ms\t{}\tms", named, captured->P95Ms);
    std::println("STAT\t{}\tmotion_p99_ms\t{}\tms", named, captured->P99Ms);
    std::println("STAT\t{}\tmotion_over_budget_frames\t{}\tframes", named, captured->OverBudget);
    std::println("STAT\t{}\tmotion_unsettled_frames\t{}\tframes", named, captured->Unsettled);
    std::println("STAT\t{}\tmotion_peak_heap_mib\t{}\tMiB", named, captured->PeakHeapMiB);
    std::println("STAT\t{}\tmotion_missing_contact_frames\t{}\tframes",
                 named,
                 captured->MissingContactFrames);
    if (captured->MinimumEyeClearanceM && captured->MaximumEyeClearanceM) {
      std::println(
          "STAT\t{}\tmotion_eye_clearance_min_m\t{}\tm", named, *captured->MinimumEyeClearanceM);
      std::println(
          "STAT\t{}\tmotion_eye_clearance_max_m\t{}\tm", named, *captured->MaximumEyeClearanceM);
    }
  }
  return captured->MissingContactFrames == 0 ? 0 : 1;
}

[[nodiscard]] bool PrintMeasures(outshine::Engine &engine) {
  auto capture = engine.beginCapture();
  if (!capture && engine.settled()) {
    std::println(stderr, "outshine-client: diagnostic capture: {}", capture.error());
    return false;
  }
  if (const auto inspected = engine.inspect(); !inspected) {
    std::println(stderr, "outshine-client: diagnostic readback: {}", inspected.error());
    return false;
  }
  for (const outshine::DiagnosticSample &one : engine.measures()) {
    std::println("        {:<56} {:14.3f} {}", one.Name, one.Value, one.Unit);
  }
  return true;
}

[[nodiscard]] int PrepareScenarioRun(outshine::Engine &engine, const ScenarioRunOptions &options) {
  if (!Stands(engine, {}, ClientRoots(options.CacheDirectory, options.Offline))) { return 2; }
  if (const auto read = engine.readScenario(options.Argv[0]); !read) {
    std::println("outshine-client: {} -- {}", options.Argv[0], read.error());
    return 1;
  }
  if (options.ProbePixel) {
    if (const auto prepared = outshine::Client::PreparePixelAttachments(engine); !prepared) {
      std::println(stderr, "outshine-client: pixel attachments: {}", prepared.error());
      return 1;
    }
  }
  outshine::Extent frame = engine.declaration().Render.Frame;
  if (frame.WidthPx <= 0 || frame.HeightPx <= 0) {
    frame = {.WidthPx = outshine::Shots::kWidePx, .HeightPx = outshine::Shots::kHighPx};
  }
  if (options.ProbePixel &&
      (options.ProbePixel->X >= frame.WidthPx || options.ProbePixel->Y >= frame.HeightPx)) {
    std::println(stderr, "outshine-client: probe pixel lies outside the current render target");
    return 2;
  }
  if (const auto targeted = engine.setRenderTarget(frame); !targeted) {
    std::println("outshine-client: {}", targeted.error());
    return 1;
  }
  if (const auto assembled = engine.assemble(); !assembled) {
    std::println("outshine-client: {} did not assemble -- {}", options.Argv[0], assembled.error());
    return 1;
  }
  return 0;
}

int RunScenario(int argc, const char *const *argv, bool everyMeasure) {
  const auto parsed = ParseScenarioRunOptions(argc, argv);
  if (!parsed) { return parsed.error(); }
  const ScenarioRunOptions &options = *parsed;
  if (options.Argc < 1) {
    std::println("outshine-client: name a scenario to run");
    return 2;
  }
  const std::string named = options.Argc > 1 ? options.Argv[1] : "scenario";
  const auto began = std::chrono::steady_clock::now();
  outshine::Engine engine;
  const auto setupFailed = [&](int result) {
    if (options.Stats) {
      const double elapsedMs =
          std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - began)
              .count();
      PrintStats(named, engine.loading(), elapsedMs, false, false, false, elapsedMs);
    }
    return result;
  };
  if (const int prepared = PrepareScenarioRun(engine, options); prepared != 0) {
    return setupFailed(prepared);
  }
  const double setupMs =
      std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - began).count();
  if (!options.SelectedView.empty()) {
    const int result = CaptureView(engine, named, options);
    if (everyMeasure && !PrintMeasures(engine)) { return 1; }
    if (options.Stats) {
      const double elapsedMs =
          std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - began)
              .count();
      PrintStats(named,
                 engine.loading(),
                 elapsedMs,
                 result == 0,
                 engine.settled(outshine::WorldQuality::Playable),
                 engine.settled(outshine::WorldQuality::Refined),
                 setupMs);
    }
    return result;
  }
  const Shot shot = outshine::Shots::Draw(engine, named, true, options.Into);
  if (options.Rows) {
    Row(shot, named);
  } else {
    Tell(shot, named);
  }
  if (everyMeasure && !PrintMeasures(engine)) { return 1; }
  if (options.Stats) {
    const double elapsedMs =
        std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - began).count();
    PrintStats(named,
               engine.loading(),
               elapsedMs,
               shot.Why.empty(),
               engine.settled(outshine::WorldQuality::Playable),
               engine.settled(outshine::WorldQuality::Refined),
               setupMs);
  }
  return shot.Why.empty() ? 0 : 1;
}

namespace Says {
constexpr auto kHeightArguments =
    "outshine-client: height requires exactly a latitude and longitude\n";
constexpr auto kHeightCoordinates =
    "outshine-client: height requires finite latitude in [-90,90] and longitude in [-180,180]\n";
}

[[nodiscard]] int QueryTerrainHeight(std::span<const char *const> arguments) {
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
  constexpr double kTerrainRequestTimeoutS = 10.0;
  constexpr double kTerrainPreloadTimeoutS = 15.0;
  stands.Ground.PatienceS = kTerrainRequestTimeoutS;
  if (const auto declared = engine.declare(stands); !declared) {
    std::println("outshine-client: the ground declaration failed -- {}", declared.error());
    return 1;
  }
  if (const auto assembled = engine.assemble(); !assembled) {
    std::println("outshine-client: the ground did not assemble -- {}", assembled.error());
    return 1;
  }
  if (const auto preloaded = engine.preload(kTerrainPreloadTimeoutS); !preloaded) {
    std::println("outshine-client: the ground did not arrive -- {}", preloaded.error());
    return 1;
  }
  const outshine::Holds<double> heightM = engine.sampleHeight(
      outshine::LongitudeLatitudeHeight{.LongitudeDeg = lon, .LatitudeDeg = lat});
  if (!heightM) {
    std::println(
        "outshine-client: no elevation stands at {:.5f} {:.5f} -- {}", lat, lon, heightM.error());
    return 1;
  }
  std::println("{:.5f} {:.5f}  {:.2f} m", lat, lon, *heightM);
  return 0;
}

std::expected<std::vector<Place>, std::string>
LoadCommandPlaces(const outshine::Client::CommandLine &command) {
  if (command.Verb == "shots" || command.Verb == "prepare" || command.Verb == "places" ||
      command.Verb == "roundtrip") {
    return outshine::Shots::LoadPlaces(command.Directory);
  }
  return std::vector<Place>{};
}

int ListPlaces(std::span<const Place> places) {
  for (const Place &one : places) {
    const auto &view = one.Declaration.Views.front().Sees;
    const auto &at = one.Declaration.Views.front().Geographic;
    const auto &frame = one.Declaration.Render.Frame;
    std::println("{}\t{:.12g}\t{:.12g}\t{:.12g}\t{:.12g}\t{:.12g}\t{:.12g}\t{}\t{}\t{}",
                 one.Name,
                 at.Geodetic.LatitudeDeg,
                 at.Geodetic.LongitudeDeg,
                 at.Geodetic.HeightM,
                 at.BearingDeg,
                 at.PitchDeg,
                 view.FovDeg,
                 frame.WidthPx,
                 frame.HeightPx,
                 one.Declaration.Time.Start);
  }
  return 0;
}

int RunClientCommand(std::span<const char *const> arguments) {
  const auto command = outshine::Client::ReadCommandLine(arguments);
  if (!command) {
    std::println(stderr, "outshine-client: {}", command.error());
    return 2;
  }
  const auto verb = command->Verb;
  const auto rest = static_cast<int>(command->Arguments.size());
  const auto *const from = command->Arguments.data();
  if (const auto help = RequestedHelp(*command); help) {
    Usage(*help);
    return 0;
  }
  auto loaded = LoadCommandPlaces(*command);
  if (!loaded) {
    std::println(stderr, "outshine-client: {}", loaded.error());
    return 1;
  }
  const auto &places = *loaded;
  if (verb == "shots") { return TakeShots(places, rest, from); }
  if (verb == "prepare" && rest == 2) {
    const auto *place = outshine::Shots::PlaceNamed(places, from[0]);
    char *end = nullptr;
    const double seconds = std::strtod(from[1], &end);
    if (place == nullptr || end == from[1] || *end != 0 || !std::isfinite(seconds) ||
        seconds <= 0) {
      return 2;
    }
    outshine::Shots::Telling = &gTelling;
    const std::string why = outshine::Shots::Prepare(*place, seconds);
    std::println(
        "PREPARE {} {}; no frame-rate measurement", place->Name, why.empty() ? "ready" : why);
    return why.empty() ? 0 : 1;
  }
  if (verb == "render") { return outshine::Client::RenderAsset({from, static_cast<size_t>(rest)}); }
  if (verb == "run") { return RunScenario(rest, from, false); }
  if (verb == "measures") { return RunScenario(rest, from, true); }
  if (verb == "height") { return QueryTerrainHeight({from, static_cast<std::size_t>(rest)}); }
  if (verb == "roundtrip") {
    return outshine::Client::RoundTripPlaces(places, "build/outshine-roundtrip.scn");
  }
  if (verb == "places") { return ListPlaces(places); }
  Usage();
  return verb == "help" || verb == "--help" ? 0 : 2;
}

}

int main(int argc, char **argv) {
  std::setvbuf(stdout, nullptr, _IONBF, 0);
  return outshine::Client::RunAtProcessBoundary(
      [arguments = std::span(argv, static_cast<size_t>(argc))] {
        return RunClientCommand(arguments);
      });
}
