#include "AzimuthElevation.h"
#include "BuildingRadianceField.h"
#include "RadianceSupport.h"
#include "ImpostorViewFrame.h"
#include "MaterialFieldInput.h"
#include "MaterialFieldPass.h"
#include "PunctualLight.h"
#include "ReadbackTransfer.h"
#include "SceneRenderer.h"
#include "device/native/Device.h"
#include "test/harness/render/NativeOffline.h"
#include <SDL3/SDL.h>
#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <expected>
#include <memory>
#include <nlohmann/json.hpp>
#include <numbers>
#include <optional>
#include <ratio>
#include <string>
#include <utility>
#include <vector>

namespace outshine::Test {
namespace {
using Clock = std::chrono::steady_clock;
constexpr auto kOfflineBudget = std::chrono::seconds(5);

double Elapsed(Clock::time_point began) {
  return std::chrono::duration<double, std::milli>(Clock::now() - began).count();
}

std::expected<Render::MaterialFieldView, std::string> ViewOf(const ImpostorAtlas &atlas,
                                                             size_t view) {
  const Vec3 direction = atlas.Views()[view].TowardEye;
  const auto frame = ImpostorViewFrame::From(direction);
  if (!frame) { return std::unexpected("source view frame is singular"); }
  Render::MaterialFieldView result;
  auto &camera = result.Camera;
  camera.EyeM = atlas.CentreM() + direction * (3 * atlas.HalfExtentM());
  camera.Right = frame->Right;
  camera.Up = frame->Up;
  camera.Forward = direction * -1.0;
  camera.Kind = Render::CameraKind::Orthographic;
  camera.XMagM = camera.YMagM = atlas.HalfExtentM();
  camera.ZNearM = atlas.HalfExtentM();
  camera.ZFarM = 5 * atlas.HalfExtentM();
  result.Target = {.WidthPx = atlas.Pixels(), .HeightPx = atlas.Pixels()};
  result.Encoding = Render::RadianceEncoding::IntegralPairs;
  return result;
}

std::expected<double, std::string> GpuMilliseconds(Render::Native::Device &device,
                                                   Render::Native::Completion produced) {
  const auto timing = device.SubmissionGpuTime(produced);
  if (!timing) { return std::unexpected(timing.error().Message); }
  if (timing->Status != Render::Native::CompletionStatus::Ready ||
      !std::isfinite(timing->ElapsedNanoseconds)) {
    return std::unexpected("material field GPU timing is not ready or finite");
  }
  return timing->ElapsedNanoseconds / 1'000'000.0;
}

std::expected<void, std::string> Shade(Render::SceneRenderer &renderer,
                                       const ImpostorAtlas &atlas,
                                       size_t view,
                                       const nlohmann::json &query,
                                       BuildingRadianceField &result,
                                       const Geometry *original) {
  const auto observer = BuildingLightingObserver(query);
  if (!observer) { return std::unexpected(observer.error()); }
  auto began = Clock::now();
  const auto configured = renderer.ConfigureMaterialFields();
  if (!configured) { return std::unexpected(configured.error().Message); }
  result.ConfigureMs = Elapsed(began);
  const bool uploaded = !result.Source;
  if (uploaded) {
    began = Clock::now();
    auto input =
        original != nullptr
            ? Render::MaterialFieldInput::Prepare(
                  original->clone(), {.WidthPx = atlas.Pixels(), .HeightPx = atlas.Pixels()})
            : Render::MaterialFieldInput::Prepare(atlas, view, size_t{64} * 1024 * 1024);
    if (!input) { return std::unexpected(input.error()); }
    result.SourcePrepareMs = Elapsed(began);
    began = Clock::now();
    auto source =
        Gpu::RetryOffline([&] { return renderer.PrepareMaterialField(*input); }, kOfflineBudget);
    if (!source) { return std::unexpected(source.error().Message); }
    result.Source = std::move(*source);
    result.SourceUploadMs = Elapsed(began);
  }
  auto placed = ViewOf(atlas, view);
  if (!placed) { return std::unexpected(placed.error()); }
  placed->LightingCamera = *observer;
  const auto toSun =
      EastUpSouthDirection(0, query.value("sun_elevation_deg", 0.0) * std::numbers::pi / 180.0);
  std::array<Render::SceneLight, 1> lights{};
  lights[0].Light.Intensity = 100;
  for (int axis = 0; axis < 3; ++axis) {
    lights[0].Light.Direction[axis] = -static_cast<float>(toSun[axis]);
  }
  placed->Lights = lights;
  began = Clock::now();
  auto images = Gpu::RetryOffline(
      [&] { return renderer.RelightMaterialField(*result.Source, *placed); }, kOfflineBudget);
  if (!images) { return std::unexpected(images.error().Message); }
  result.RelightEncodeMs = Elapsed(began);
  result.Radiance = images->Filtered;
  result.SourceCamera = images->Camera;
  result.LightingCamera = images->LightingCamera;
  began = Clock::now();
  renderer.WaitForGpu();
  result.WaitMs = Elapsed(began);
  auto &device = *result.Radiance->Device;
  const auto gpu = GpuMilliseconds(device, result.Radiance->Produced);
  if (!gpu) { return std::unexpected(gpu.error()); }
  result.GpuMs = *gpu;
  if (uploaded) {
    const auto setup = GpuMilliseconds(device, result.Source->Produced());
    if (!setup) { return std::unexpected(setup.error()); }
    result.SetupGpuMs = *setup;
  }
  if (query.value("dump_source_field", true)) {
    began = Clock::now();
    auto pixels = ReadMaterialFieldImage(*images->Unfiltered->Device, *images->Unfiltered->Texture);
    if (!pixels) { return std::unexpected(pixels.error()); }
    auto depths = ReadMaterialFieldImage(*images->Unfiltered->Device, *images->Depth);
    if (!depths) { return std::unexpected(depths.error()); }
    result.SourceLinear = std::move(*pixels);
    result.SourceDepth = std::move(*depths);
    result.ReadbackMs = Elapsed(began);
  }
  return {};
}
} // namespace

Render::Viewpoint BuildingCaptureCamera(const nlohmann::json &description) {
  Render::Viewpoint camera;
  camera.EyeM = Vec3{description.at("eye_m").get<std::array<double, 3>>()};
  camera.Right = Vec3{description.at("right").get<std::array<double, 3>>()};
  camera.Up = Vec3{description.at("up").get<std::array<double, 3>>()};
  camera.Forward = Vec3{description.at("forward").get<std::array<double, 3>>()};
  camera.Kind = description.at("orthographic").get<bool>() ? Render::CameraKind::Orthographic
                                                           : Render::CameraKind::Perspective;
  camera.XMagM = description.at("x_mag_m").get<double>();
  camera.YMagM = description.at("y_mag_m").get<double>();
  camera.YfovRad = description.at("yfov_rad").get<double>();
  camera.ZNearM = description.at("near_m").get<double>();
  camera.ZFarM = description.at("far_m").get<double>();
  return camera;
}

Render::Viewpoint BuildingQueryCamera(const nlohmann::json &query) {
  if (query.contains("capture")) { return BuildingCaptureCamera(query.at("capture")); }
  Render::Viewpoint camera;
  camera.EyeM = Vec3{query.at("eye").get<std::array<double, 3>>()};
  camera.Right = Vec3{query.at("basis").at(0).get<std::array<double, 3>>()};
  camera.Up = Vec3{query.at("basis").at(1).get<std::array<double, 3>>()};
  camera.Forward = Vec3{query.at("basis").at(2).get<std::array<double, 3>>()};
  camera.YfovRad = 2 * std::atan(360.0 / query.at("focal_px").get<double>());
  camera.ZNearM = .1;
  return camera;
}

std::expected<std::optional<Render::Viewpoint>, std::string>
BuildingLightingObserver(const nlohmann::json &query) {
  const auto mode = query.value("field_lighting", "capture");
  if (mode != "capture" && mode != "viewer") {
    return std::unexpected("unknown material field lighting observer");
  }
  const auto &description =
      mode == "viewer" && query.contains("lighting_camera") ? query.at("lighting_camera") : query;
  const auto camera = BuildingQueryCamera(description);
  if (!Render::ValidMaterialFieldCameraBasis(camera)) {
    return std::unexpected("material field query camera has an invalid basis");
  }
  if (mode == "viewer") { return camera; }
  return std::nullopt;
}

std::expected<std::vector<float>, std::string>
ReadMaterialFieldImage(Render::Native::Device &device, const Render::Native::Texture &image) {
  const auto &shape = image.Description();
  const bool depth = shape.Texels == Render::Native::Format::Depth32Float;
  if (!depth && shape.Texels != Render::Native::Format::Rgba16Float &&
      shape.Texels != Render::Native::Format::Rgba32Float) {
    return std::unexpected("source field has an unsupported readback format");
  }
  auto transfer = Gpu::RetryOffline(
      [&] {
        return Render::ReadbackTransfer::Start(
            device, image, {.Width = shape.Width, .Height = shape.Height});
      },
      kOfflineBudget);
  if (!transfer) { return std::unexpected(transfer.error().Message); }
  const auto pixels = transfer->Wait(device, kOfflineBudget);
  if (!pixels) { return std::unexpected(pixels.error().Message); }
  if (!*pixels) { return std::unexpected("source radiance readback exceeded its offline budget"); }
  std::vector<float> result(size_t{shape.Width} * shape.Height * (depth ? 1 : 4));
  if (depth || shape.Texels == Render::Native::Format::Rgba32Float) {
    const size_t rowFloats = size_t{shape.Width} * (depth ? 1 : 4);
    for (size_t row = 0; row < shape.Height; ++row) {
      std::memcpy(result.data() + row * rowFloats,
                  (*pixels)->data() + row * transfer->RowBytes(),
                  rowFloats * sizeof(float));
    }
  } else if (!SDL_ConvertPixels(static_cast<int>(shape.Width),
                                static_cast<int>(shape.Height),
                                SDL_PIXELFORMAT_RGBA64_FLOAT,
                                (*pixels)->data(),
                                static_cast<int>(transfer->RowBytes()),
                                SDL_PIXELFORMAT_RGBA128_FLOAT,
                                result.data(),
                                static_cast<int>(shape.Width * 16))) {
    return std::unexpected("source radiance field is incomplete or nonfinite");
  }
  if (!std::ranges::all_of(result, [](float x) { return std::isfinite(x); })) {
    return std::unexpected("material field readback is nonfinite");
  }
  return result;
}

std::expected<void, std::string> UpdateBuildingRadianceSupport(BuildingRadianceField &field,
                                                               const ImpostorAtlas &atlas,
                                                               size_t view,
                                                               const nlohmann::json &query) {
  field.SupportFitMs = 0;
  field.SupportDepth = {};
  const auto mode = query.value("support_plane", "capture");
  if (mode == "fitted") {
    if (!field.SupportFit) {
      const auto began = Clock::now();
      auto fit = Render::FitRadiancePlane(
          atlas,
          {.View = view, .TexelsMost = (size_t{64} * 1024 * 1024) / sizeof(ImpostorAtlas::Texel)});
      if (!fit) { return std::unexpected(fit.error()); }
      field.SupportFitMs = Elapsed(began);
      field.SupportFit = *fit;
    }
    field.SupportDepth = field.SupportFit->DepthCoefficients;
  } else if (mode != "capture") {
    return std::unexpected("unknown radiance support plane mode");
  }
  const auto camera = BuildingQueryCamera(query);
  const auto lens = Render::Lens::From(camera, 1280, 720);
  if (!lens) { return std::unexpected("radiance viewer has no valid lens"); }
  auto surface = Render::BuildProjectedRadianceSupport({.Centre = atlas.CentreM(),
                                                        .TowardEye = atlas.Views()[view].TowardEye,
                                                        .HalfExtentM = atlas.HalfExtentM(),
                                                        .DepthCoefficients = field.SupportDepth},
                                                       camera,
                                                       *lens);
  if (!surface) { return std::unexpected(surface.error()); }
  field.Cards.Centre = atlas.CentreM();
  field.Cards.HalfExtentM = atlas.HalfExtentM();
  field.Cards.Views.clear();
  field.Cards.Views.push_back(
      {.Direction = atlas.Views()[view].TowardEye, .Surface = std::move(*surface)});
  return {};
}

std::expected<BuildingRadianceField, std::string>
PrepareBuildingRadiance(Render::SceneRenderer &renderer,
                        const ImpostorAtlas &atlas,
                        size_t view,
                        const nlohmann::json &query,
                        const Geometry *original,
                        const BuildingRadianceField *resident) {
  const auto began = Clock::now();
  BuildingRadianceField result;
  if (resident != nullptr) {
    result.Source = resident->Source;
    result.SupportFit = resident->SupportFit;
  }
  const auto shaded = Shade(renderer, atlas, view, query, result, original);
  if (!shaded) { return std::unexpected(shaded.error()); }
  const auto placed = UpdateBuildingRadianceSupport(result, atlas, view, query);
  if (!placed) { return std::unexpected(placed.error()); }
  result.PrepareMs = Elapsed(began);
  return result;
}
} // namespace outshine::Test
