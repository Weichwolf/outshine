#include "MetalProductBenchmark.h"
#include "BenchmarkOptions.h"
#include <expected>
#include <string>

#ifdef __APPLE__
#include "BenchmarkInput.h"
#include "BenchmarkMotion.h"
#include "BenchmarkReport.h"
#include "GeodeticCamera.h"
#include "MetalProductRenderer.h"
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <string_view>
#include <optional>
#include <utility>
#endif

namespace outshine::App {
#ifdef __APPLE__
namespace {
constexpr double kPoseRoundoffM = 1e-6;

std::expected<Camera, std::string> TileCamera(const Scenario::Document &document,
                                              std::string_view selected,
                                              LongitudeLatitude origin) {
  const auto view = std::ranges::find(document.Views, selected, &Scenario::View::Id);
  if (view == document.Views.end() || view->Placement != Scenario::CameraPlacement::Geodetic ||
      view->Geographic.SamplesHeight || view->Sees.LooksAt || view->OffsetM != Vec3{}) {
    return std::unexpected(
        "Metal product capture requires a geographic camera with absolute height, "
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

void CaptureProductSamples(ScenarioBenchmarkReport &report,
                           const Render::MetalProductFrames &frames) {
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

std::expected<Render::PanoramaView, std::string>
MakePanoramaView(const Camera &pose, Extent frame, const FarPanorama &panorama) {
  const double aspect = static_cast<double>(frame.WidthPx) / frame.HeightPx;
  Mat4 basis;
  Mat4 projection;
  if (pose.Orthographic || !pose.modelMatrix(basis) || !pose.projectionMatrix(aspect, projection)) {
    return std::unexpected("far panorama requires a valid perspective camera");
  }
  const auto offset = pose.PositionM - panorama.AnchorM;
  return Render::PanoramaView{
      .Forward = {{-basis[8], -basis[9], -basis[10]}},
      .RightSpan = Vec3{{basis[0], basis[1], basis[2]}} * (1.0 / projection[0]),
      .UpSpan = Vec3{{basis[4], basis[5], basis[6]}} * (1.0 / projection[5]),
      .EyeOffsetM = Length(offset) > kPoseRoundoffM ? offset : Vec3{},
      .FocalPixels = static_cast<double>(frame.HeightPx) * projection[5] * 0.5,
      .DepthScaleM = -projection[14] * 0.5,
      .DepthBias = -(1.0 + projection[10]) * 0.5,
      .Exposure = panorama.ReferenceExposure};
}

bool CoversMotion(const FarPanorama &panorama,
                  const ScenarioBenchmarkReport &report,
                  LongitudeLatitude origin) {
  const double radius = panorama.MaximumTranslationM + kPoseRoundoffM;
  return panorama.Origin.LongitudeDeg == origin.LongitudeDeg &&
         panorama.Origin.LatitudeDeg == origin.LatitudeDeg &&
         Length(report.Start.PositionM - panorama.AnchorM) <= radius &&
         Length(report.Finish.PositionM - panorama.AnchorM) <= radius;
}

std::expected<Render::MetalProductFrames, std::string>
MeasureProducts(const ScenarioBenchmarkOptions &options,
                ScenarioBenchmarkReport &report,
                const BenchmarkMotion &motion,
                LongitudeLatitude origin) {
  const auto loadAt = BenchmarkClock::now();
  std::optional<SurfaceTile> tile;
  std::optional<FarPanorama> panorama;
  Mat4 placement;
  if (!options.TileProduct.empty()) {
    auto input = ReadSurfaceTile(options.TileProduct);
    if (!input) { return std::unexpected(input.error()); }
    tile = std::move(*input);
    placement = TilePlacement(*tile, origin);
    report.Tile = BenchmarkTileCoverage{
        .Address = {static_cast<uint32_t>(tile->Coverage.Zoom), tile->Coverage.X, tile->Coverage.Y},
        .OriginLonLat = {tile->Origin.LongitudeDeg, tile->Origin.LatitudeDeg},
        .Revision = tile->Revision,
        .Triangles = tile->Indices.size() / 3,
        .GeometricErrorM = tile->GeometricErrorM,
        .StoredBytes = tile->StoredBytes};
  }
  if (!options.PanoramaProduct.empty()) {
    auto input = ReadFarPanorama(options.PanoramaProduct);
    if (!input) { return std::unexpected(input.error()); }
    panorama = std::move(*input);
    if (!CoversMotion(*panorama, report, origin)) {
      return std::unexpected("camera path exceeds the prepared far panorama translation domain");
    }
    report.Panorama =
        BenchmarkPanoramaCoverage{.Side = panorama->Side,
                                  .AnchorM = panorama->AnchorM,
                                  .Revision = panorama->Revision,
                                  .MaximumRangeM = panorama->MaximumRangeM,
                                  .MaximumTranslationM = panorama->MaximumTranslationM,
                                  .StoredBytes = panorama->Colour.size() + panorama->Depth.size()};
  }
  const double aspect = static_cast<double>(report.Frame.WidthPx) / report.Frame.HeightPx;
  const auto viewAt = [&](size_t frame) -> std::expected<Render::MetalProductView, std::string> {
    const auto pose = motion.At(frame);
    if (!pose) { return std::unexpected(std::string(pose.error())); }
    Render::MetalProductView view{};
    if (tile) {
      Mat4 clip;
      if (!pose->clipMatrix(aspect, clip)) { return std::unexpected("invalid tile camera"); }
      view.TileClip = clip * placement;
    }
    if (panorama) {
      const auto projected = MakePanoramaView(*pose, report.Frame, *panorama);
      if (!projected) { return std::unexpected(projected.error()); }
      view.Panorama = *projected;
    }
    return view;
  };
  report.PreloadMs = BenchmarkElapsedMs(loadAt);
  return Render::RenderMetalProducts(
      {.Tile = tile ? &*tile : nullptr, .Panorama = panorama ? &*panorama : nullptr},
      report.Motion.Frames,
      viewAt,
      !options.Image.empty());
}

}
#endif

std::expected<std::string, std::string>
RunMetalProductBenchmark(const ScenarioBenchmarkOptions &options) {
#ifdef __APPLE__
  ScenarioBenchmarkReport report;
  const auto setupAt = BenchmarkClock::now();
  CaptureBenchmarkHost(report, true);
  const auto document = ReadBenchmarkScenario(options, "data", report);
  if (!document) { return std::unexpected(document.error()); }
  if (report.Frame != Extent{.WidthPx = Render::MetalProductFrameWidth,
                             .HeightPx = Render::MetalProductFrameHeight}) {
    return std::unexpected("Metal product capture requires a 1280x720 scenario");
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

  const auto frames = MeasureProducts(options, report, *motion, origin);
  if (!frames) { return std::unexpected(frames.error()); }
  report.ProductUploadBytes = frames->UploadedBytes;
  report.ProductUploadMs = frames->UploadMs;
  report.SetupMs += frames->SetupMs;
  report.PreloadMs += frames->UploadMs;
  CaptureProductSamples(report, *frames);
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
  return std::unexpected("product capture requires the Metal backend");
#endif
}

}
