#include "TileBenchmark.h"
#include "BenchmarkOptions.h"
#include <expected>
#include <string>

#ifdef __APPLE__
#include "BenchmarkInput.h"
#include "BenchmarkMotion.h"
#include "BenchmarkReport.h"
#include "GeodeticCamera.h"
#include "MetalTileRenderer.h"
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <string_view>
#endif

namespace outshine::App {
#ifdef __APPLE__
namespace {

std::expected<Camera, std::string> TileCamera(const Scenario::Document &document,
                                              std::string_view selected,
                                              LongitudeLatitude origin) {
  const auto view = std::ranges::find(document.Views, selected, &Scenario::View::Id);
  if (view == document.Views.end() || view->Placement != Scenario::CameraPlacement::Geodetic ||
      view->Geographic.SamplesHeight || view->Sees.LooksAt || view->OffsetM != Vec3{}) {
    return std::unexpected("tile capture requires a geographic camera with absolute height, "
                           "bearing and pitch");
  }
  const auto position = ResolveGeodeticCamera(*view, origin, nullptr);
  const auto axes = ResolveGeodeticCameraAxes(view->Geographic, origin);
  if (!position || !*position || !axes) { return std::unexpected("invalid tile capture camera"); }
  auto camera = view->Sees;
  camera.PositionM = **position;
  camera.LooksAt = true;
  camera.LookAtM = camera.PositionM + axes->Forward;
  camera.UpM = axes->Up;
  if (!camera.Orthographic) {
    if (camera.NearM == 0) { camera.NearM = Camera::kNearestM; }
    if (camera.FovDeg == 0) { camera.FovDeg = Scenario::kFovUnsaidDeg; }
  }
  return camera;
}

Mat4 TilePlacement(const SurfaceTile &tile, LongitudeLatitude origin) {
  const auto world = TangentFrame::At(origin);
  const auto local = TangentFrame::At(tile.Origin);
  const auto position = RenderFrame::Of(world.ToLocalPosition(
      {.LongitudeDeg = tile.Origin.LongitudeDeg, .LatitudeDeg = tile.Origin.LatitudeDeg}));
  const auto east = RenderFrame::Of(world.ToLocalDirection(local.EastEcef()));
  const auto up = RenderFrame::Of(world.ToLocalDirection(local.UpEcef()));
  const auto south = RenderFrame::Of(world.ToLocalDirection(local.NorthEcef() * -1.0));
  return {{east[0],
           east[1],
           east[2],
           0,
           up[0],
           up[1],
           up[2],
           0,
           south[0],
           south[1],
           south[2],
           0,
           position[0],
           position[1],
           position[2],
           1}};
}

void CaptureTileSamples(ScenarioBenchmarkReport &report, const Render::MetalTileFrames &frames) {
  report.WindowMs = frames.WindowMs;
  report.FlushMs = frames.FlushMs;
  report.Gpu = frames.Device;
  report.GpuMemory.ResourceAllocatedBytes = frames.ResourceBytes;
  report.GpuMemory.RecommendedWorkingSetBytes = frames.RecommendedResourceBytes;
  report.GpuMemory.UnifiedMemory = frames.UnifiedMemory;
  report.GpuTiming.RenderedFrames = static_cast<double>(frames.Samples.size());
  report.Frames.reserve(frames.Samples.size());
  report.GpuSamples.reserve(frames.Samples.size());
  for (size_t frame = 0; frame < frames.Samples.size(); ++frame) {
    const auto &sample = frames.Samples[frame];
    report.Frames.push_back({.WallMs = sample.WallMs,
                             .RenderMs = sample.RenderMs,
                             .SyncWaitMs = sample.SyncWaitMs,
                             .Refined = true});
    if (sample.GpuMs) {
      report.GpuSamples.push_back(
          {.Device = frames.DeviceId, .Submission = frame + 1, .ElapsedMs = *sample.GpuMs});
      ++report.GpuTiming.TimedFrames;
      report.GpuTiming.TotalMs += *sample.GpuMs;
      report.GpuTiming.MaxMs = std::max(report.GpuTiming.MaxMs, *sample.GpuMs);
    } else {
      ++report.GpuTiming.UnavailableFrames;
    }
  }
}
}
#endif

std::expected<std::string, std::string> RunTileBenchmark(const ScenarioBenchmarkOptions &options) {
#ifdef __APPLE__
  ScenarioBenchmarkReport report;
  const auto setupAt = BenchmarkClock::now();
  CaptureBenchmarkHost(report, true);
  const auto document = ReadBenchmarkScenario(options, "data", report);
  if (!document) { return std::unexpected(document.error()); }
  if (report.Frame !=
      Extent{.WidthPx = Render::TileFrameWidth, .HeightPx = Render::TileFrameHeight}) {
    return std::unexpected("tile capture requires a 1280x720 scenario");
  }
  const LongitudeLatitude origin{.LongitudeDeg = document->Ground.Origin.LongitudeDeg,
                                 .LatitudeDeg = document->Ground.Origin.LatitudeDeg};
  const auto camera = TileCamera(*document, report.View, origin);
  if (!camera) { return std::unexpected(camera.error()); }
  report.Start = *camera;
  const auto motion = BenchmarkMotion::Make(report.Start, report.Motion);
  if (!motion) { return std::unexpected(std::string(motion.error())); }
  const auto finish = motion->At(report.Motion.Frames - 1);
  if (!finish) { return std::unexpected(std::string(finish.error())); }
  report.Finish = *finish;
  report.SetupMs = BenchmarkElapsedMs(setupAt);

  const auto loadAt = BenchmarkClock::now();
  const auto tile = ReadSurfaceTile(options.TileProduct);
  if (!tile) { return std::unexpected(tile.error()); }
  const auto placement = TilePlacement(*tile, origin);
  const double aspect = static_cast<double>(report.Frame.WidthPx) / report.Frame.HeightPx;
  const auto clipAt = [&](size_t frame) -> std::expected<Mat4, std::string> {
    const auto pose = motion->At(frame);
    Mat4 clip;
    if (!pose || !pose->clipMatrix(aspect, clip)) { return std::unexpected("invalid tile camera"); }
    return clip * placement;
  };
  report.PreloadMs = BenchmarkElapsedMs(loadAt);
  const auto frames =
      Render::RenderMetalTile(*tile, report.Motion.Frames, clipAt, !options.Image.empty());
  if (!frames) { return std::unexpected(frames.error()); }
  report.SetupMs += frames->SetupMs;
  report.PreloadMs += frames->UploadMs;
  report.Tile = BenchmarkTileCoverage{
      .Address = {static_cast<uint32_t>(tile->Coverage.Zoom), tile->Coverage.X, tile->Coverage.Y},
      .OriginLonLat = {tile->Origin.LongitudeDeg, tile->Origin.LatitudeDeg},
      .Revision = tile->Revision,
      .Triangles = tile->Indices.size() / 3,
      .GeometricErrorM = tile->GeometricErrorM,
      .StoredBytes = tile->StoredBytes,
      .UploadBytes = frames->UploadedBytes,
      .UploadMs = frames->UploadMs};
  CaptureTileSamples(report, *frames);
  if (!options.Image.empty()) {
    const auto imageAt = BenchmarkClock::now();
    if (const auto saved = SaveBenchmarkImage(report, frames->Rgba, options.Image); !saved) {
      return std::unexpected(saved.error());
    }
    report.ImageMs = BenchmarkElapsedMs(imageAt);
  }
  const auto hardwareAt = BenchmarkClock::now();
  CaptureBenchmarkHost(report, false);
  report.HardwareMs = BenchmarkElapsedMs(hardwareAt);
  return ScenarioBenchmarkJson(report);
#else
  static_cast<void>(options);
  return std::unexpected("tile capture requires the Metal backend");
#endif
}

}
