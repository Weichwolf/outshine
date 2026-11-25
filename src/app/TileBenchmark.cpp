#include "TileBenchmark.h"
#include "BenchmarkMotion.h"
#include "BenchmarkOptions.h"

#ifdef __APPLE__
#include "GeodeticCamera.h"
#include "ImageCapture.h"
#include "MetalTileRenderer.h"
#include "Quantile.h"
#include "ScenarioInput.h"
#include "ScenarioWrite.h"
#include "format/Sha256.h"
#include <algorithm>
#include <chrono>
#include <cstddef>
#include <expected>
#include <ratio>
#include <string>
#include <string_view>
#include <nlohmann/json.hpp>
#include <vector>
#endif

namespace outshine::App {
#ifdef __APPLE__
namespace {
using Clock = std::chrono::steady_clock;

std::expected<Camera, std::string> TileCamera(const Scenario::Document &document,
                                              std::string_view selected,
                                              LongitudeLatitude origin) {
  if (selected.empty()) { selected = document.Played.View; }
  if (selected.empty() && !document.Views.empty()) { selected = document.Views.front().Id; }
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

BenchmarkMotionInput RebaseMotion(BenchmarkMotionInput motion,
                                  const Scenario::Georeference &world,
                                  const SurfaceTile &tile) {
  const auto from =
      TangentFrame::At({.LongitudeDeg = world.LongitudeDeg, .LatitudeDeg = world.LatitudeDeg});
  const auto into = TangentFrame::At(tile.Origin);
  const auto apply = [&](const Vec3 &direction) {
    const Vec3 ecef = from.EastEcef() * direction[0] + from.UpEcef() * direction[1] -
                      from.NorthEcef() * direction[2];
    return Vec3{RenderFrame::Of(into.ToLocalDirection(ecef))};
  };
  motion.LinearVelocityMps = apply(motion.LinearVelocityMps);
  motion.AngularVelocityDegS = apply(motion.AngularVelocityDegS);
  return motion;
}

std::expected<std::vector<Camera>, std::string> TileCameraFrames(const Camera &initial,
                                                                 BenchmarkMotionInput input) {
  const auto motion = BenchmarkMotion::Make(initial, input);
  if (!motion) { return std::unexpected(std::string(motion.error())); }
  std::vector<Camera> cameras;
  cameras.reserve(input.Frames);
  for (size_t frame = 0; frame < input.Frames; ++frame) {
    const auto camera = motion->At(frame);
    if (!camera) { return std::unexpected(std::string(camera.error())); }
    cameras.push_back(*camera);
  }
  return cameras;
}

nlohmann::json TileCameraReport(const Camera &camera) {
  Mat4 model;
  if (!camera.modelMatrix(model)) { return nullptr; }
  return {{"position_m", camera.PositionM.Axis},
          {"world_from_camera", model.Column},
          {"perspective_fov_deg", camera.FovDeg},
          {"near_m", camera.NearM},
          {"far_m", camera.FarM}};
}
}
#endif

std::expected<std::string, std::string> RunTileBenchmark(const ScenarioBenchmarkOptions &options) {
#ifdef __APPLE__
  const auto began = Clock::now();
  const auto document = ReadScenarioInput(options.Source, options.Overrides, "data");
  if (!document) { return std::unexpected(document.error()); }
  constexpr Extent kFrame{.WidthPx = Render::TileFrameWidth, .HeightPx = Render::TileFrameHeight};
  if (document->Render.Frame != Extent{} && document->Render.Frame != kFrame) {
    return std::unexpected("tile capture requires a 1280x720 scenario");
  }
  const auto declaration = WriteScenario(*document);
  if (!declaration) { return std::unexpected(declaration.error()); }
  const auto tile = ReadSurfaceTile(options.TileProduct);
  if (!tile) { return std::unexpected(tile.error()); }
  const auto camera = TileCamera(*document, options.View, tile->Origin);
  if (!camera) { return std::unexpected(camera.error()); }
  auto motion = RebaseMotion(options.Motion, document->Ground.Origin, *tile);
  motion.RateHz = document->Render.FrameRateHz;
  const auto cameras = TileCameraFrames(*camera, motion);
  if (!cameras) { return std::unexpected(cameras.error()); }
  const double sourceMs = std::chrono::duration<double, std::milli>(Clock::now() - began).count();
  auto frames = Render::RenderMetalTile(*tile, *cameras, !options.Image.empty());
  if (!frames) { return std::unexpected(frames.error()); }
  if (!options.Image.empty()) {
    if (const auto saved = SaveImage(frames->Rgba, kFrame, options.Image); !saved) {
      return std::unexpected(saved.error());
    }
  }
  auto samples = frames->GpuMs;
  std::ranges::sort(samples);
  const nlohmann::json report{
      {"backend", "Metal"},
      {"device", frames->Device},
      {"frame_px", {kFrame.WidthPx, kFrame.HeightPx}},
      {"rate_hz", motion.RateHz},
      {"linear_velocity_mps", options.Motion.LinearVelocityMps.Axis},
      {"angular_velocity_deg_s", options.Motion.AngularVelocityDegS.Axis},
      {"frames", samples.size()},
      {"declaration_sha256", Sha256Hex(*declaration)},
      {"coverage", {tile->Coverage.Zoom, tile->Coverage.X, tile->Coverage.Y}},
      {"origin_lon_lat", {tile->Origin.LongitudeDeg, tile->Origin.LatitudeDeg}},
      {"tile_revision", tile->Revision},
      {"geometric_error_m", tile->GeometricErrorM},
      {"triangles", tile->Indices.size() / 3},
      {"start_camera", TileCameraReport(*camera)},
      {"final_camera", TileCameraReport(cameras->back())},
      {"startup_ms", sourceMs + frames->SetupMs + frames->UploadMs},
      {"input_ms", sourceMs},
      {"metal_setup_ms", frames->SetupMs},
      {"upload_ms", frames->UploadMs},
      {"uploaded_bytes", frames->UploadedBytes},
      {"gpu_ms",
       {{"samples", frames->GpuMs},
        {"p50", *QuantileOf(samples, kMiddleQuantile)},
        {"p99", *QuantileOf(samples, kWidestQuantile)}}}};
  return report.dump() + '\n';
#else
  static_cast<void>(options);
  return std::unexpected("tile capture requires the Metal backend");
#endif
}

}
