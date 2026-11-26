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
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <string_view>
#include <optional>
#include <utility>
#include <vector>
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

Vec3 PanoramaOffset(const Camera &pose, const FarPanorama &panorama) {
  const auto offset = pose.PositionM - panorama.AnchorM;
  return Length(offset) > kPoseRoundoffM ? offset : Vec3{};
}

std::expected<Render::PanoramaView, std::string>
MakePanoramaView(const Camera &pose, Extent frame, const FarPanorama &panorama) {
  const double aspect = static_cast<double>(frame.WidthPx) / frame.HeightPx;
  Mat4 basis;
  Mat4 projection;
  if (pose.Orthographic || !pose.modelMatrix(basis) || !pose.projectionMatrix(aspect, projection)) {
    return std::unexpected("far panorama requires a valid perspective camera");
  }
  return Render::PanoramaView{
      .Forward = {{-basis[8], -basis[9], -basis[10]}},
      .RightSpan = Vec3{{basis[0], basis[1], basis[2]}} * (1.0 / projection[0]),
      .UpSpan = Vec3{{basis[4], basis[5], basis[6]}} * (1.0 / projection[5]),
      .EyeOffsetM = PanoramaOffset(pose, panorama),
      .FocalPixels = static_cast<double>(frame.HeightPx) * projection[5] * 0.5,
      .DepthScaleM = -projection[14] * 0.5,
      .DepthBias = -(1.0 + projection[10]) * 0.5,
      .Exposure = panorama.ReferenceExposure};
}

bool CoversEye(const FarPanorama &panorama, const Vec3 &eye) {
  return Length(eye - panorama.AnchorM) <= panorama.MaximumTranslationM + kPoseRoundoffM;
}

double ChordHalfLength(const FarPanorama &panorama, const Vec3 &axisOffset) {
  const double radius = panorama.MaximumTranslationM + kPoseRoundoffM;
  return std::sqrt(std::max(0.0, radius * radius - Dot(axisOffset, axisOffset)));
}

std::expected<std::array<double, 2>, std::string> PanoramaOverlap(
    const FarPanorama &first, const FarPanorama &next, const ScenarioBenchmarkReport &report) {
  const Vec3 start = report.Start.PositionM;
  const Vec3 finish = report.Finish.PositionM;
  const double length = Length(finish - start);
  if (length <= kPoseRoundoffM || !CoversEye(first, start) || !CoversEye(next, finish)) {
    return std::unexpected("panorama handoff requires a flight covered at both endpoints");
  }
  if (first.Side != next.Side || first.MaximumRangeM != next.MaximumRangeM ||
      first.ReferenceExposure != next.ReferenceExposure) {
    return std::unexpected("panorama handoff requires matching resolution, range and exposure");
  }
  const Vec3 direction = (finish - start) * (1.0 / length);
  const Vec3 a = first.AnchorM - start;
  const Vec3 b = next.AnchorM - start;
  const double alongA = Dot(a, direction);
  const double alongB = Dot(b, direction);
  const double exit = alongA + ChordHalfLength(first, a - direction * alongA);
  const double entry = alongB - ChordHalfLength(next, b - direction * alongB);
  if (exit < entry) {
    return std::unexpected("prepared panorama domains leave a gap in the flight");
  }
  const double framesPerMetre = static_cast<double>(report.Motion.Frames) / length;
  return {{entry * framesPerMetre, exit * framesPerMetre}};
}

BenchmarkPanoramaCoverage PanoramaCoverage(const FarPanorama &panorama) {
  return {.Side = panorama.Side,
          .AnchorM = panorama.AnchorM,
          .Revision = panorama.Revision,
          .MaximumRangeM = panorama.MaximumRangeM,
          .MaximumTranslationM = panorama.MaximumTranslationM,
          .StoredBytes = panorama.Colour.size() + panorama.Depth.size()};
}

struct PreparedPanoramas {
  std::vector<FarPanorama> Products;
  std::optional<std::array<double, 2>> BlendWindowFrames;

  [[nodiscard]] double NextWeight(size_t frame) const {
    if (!BlendWindowFrames) { return 0; }
    const auto [begin, end] = *BlendWindowFrames;
    const auto sample = static_cast<double>(frame + 1);
    if (sample >= end) { return 1; }
    if (sample <= begin) { return 0; }
    const double t = (sample - begin) / (end - begin);
    return t * t * (3.0 - 2.0 * t);
  }
};

std::expected<PreparedPanoramas, std::string>
PreparePanoramas(const ScenarioBenchmarkOptions &options,
                 ScenarioBenchmarkReport &report,
                 LongitudeLatitude origin) {
  PreparedPanoramas prepared;
  for (const auto path : {options.PanoramaProduct, options.NextPanorama}) {
    if (path.empty()) { continue; }
    auto input = ReadFarPanorama(path);
    if (!input) { return std::unexpected(input.error()); }
    if (input->Origin.LongitudeDeg != origin.LongitudeDeg ||
        input->Origin.LatitudeDeg != origin.LatitudeDeg) {
      return std::unexpected("prepared panorama must share the scenario origin");
    }
    prepared.Products.push_back(std::move(*input));
  }
  if (prepared.Products.empty()) { return prepared; }
  const auto &first = prepared.Products.front();
  report.Panorama = PanoramaCoverage(first);
  if (prepared.Products.size() == 2) {
    const auto &next = prepared.Products.back();
    const auto overlap = PanoramaOverlap(first, next, report);
    if (!overlap) { return std::unexpected(overlap.error()); }
    prepared.BlendWindowFrames = *overlap;
    report.PanoramaHandoff = {.Next = PanoramaCoverage(next), .BlendWindowFrames = *overlap};
  } else if (!CoversEye(first, report.Start.PositionM) ||
             !CoversEye(first, report.Finish.PositionM)) {
    return std::unexpected("camera path exceeds the prepared far panorama translation domain");
  }
  return prepared;
}

std::expected<Render::MetalProductFrames, std::string>
MeasureProducts(const ScenarioBenchmarkOptions &options,
                ScenarioBenchmarkReport &report,
                const BenchmarkMotion &motion,
                LongitudeLatitude origin) {
  const auto loadAt = BenchmarkClock::now();
  std::optional<SurfaceTile> tile;
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
  const auto prepared = PreparePanoramas(options, report, origin);
  if (!prepared) { return std::unexpected(prepared.error()); }
  report.ProductReadMs = BenchmarkElapsedMs(loadAt);
  const auto &panoramas = prepared->Products;
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
    if (!panoramas.empty()) {
      const double blend = prepared->NextWeight(frame);
      view.PanoramaIndex = blend >= 1 ? 1 : 0;
      const auto projected = MakePanoramaView(*pose, report.Frame, panoramas[view.PanoramaIndex]);
      if (!projected) { return std::unexpected(projected.error()); }
      view.Panorama = *projected;
      if (blend > 0 && blend < 1) {
        view.Panorama.NextEyeOffsetM = PanoramaOffset(*pose, panoramas.back());
        view.Panorama.NextWeight = blend;
      }
    }
    return view;
  };
  report.PreloadMs = BenchmarkElapsedMs(loadAt);
  return Render::RenderMetalProducts({.Tile = tile ? &*tile : nullptr, .Panoramas = panoramas},
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
