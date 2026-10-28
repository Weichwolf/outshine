#include "AzimuthElevation.h"
#include "BuildingRadianceField.h"
#include "ImpostorSurface.h"
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
#include <expected>
#include <memory>
#include <nlohmann/json.hpp>
#include <numbers>
#include <ratio>
#include <optional>
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

std::expected<Render::MaterialFieldView, std::string>
ViewOf(const ImpostorAtlas &atlas, size_t view, const nlohmann::json &query) {
  const Vec3 direction = atlas.Views()[view].TowardEye;
  const auto frame = ImpostorViewFrame::From(direction);
  if (!frame) { return std::unexpected("source view frame is singular"); }
  const Vec3 eye{query.at("eye").get<std::array<double, 3>>()};
  Render::MaterialFieldView result;
  auto &camera = result.Camera;
  camera.EyeM = atlas.CentreM() + direction * Length(eye - atlas.CentreM());
  camera.Right = frame->Right;
  camera.Up = frame->Up;
  camera.Forward = direction * -1.0;
  camera.Kind = Render::CameraKind::Orthographic;
  camera.XMagM = camera.YMagM = atlas.HalfExtentM();
  camera.ZNearM = .1;
  camera.ZFarM = Length(camera.EyeM - atlas.CentreM()) + 4 * atlas.HalfExtentM();
  const double radius = std::numbers::sqrt3 * atlas.HalfExtentM();
  const double diameterPx = 2 * radius * query.at("focal_px").get<double>() /
                            std::max(1.0, Length(eye - atlas.CentreM()) - radius);
  int pixels = atlas.Pixels();
  while (pixels > 1) {
    const int next = pixels / 2;
    if (static_cast<double>(next) < diameterPx) { break; }
    pixels = next;
  }
  result.Target = {.WidthPx = pixels, .HeightPx = pixels};
  return result;
}

std::expected<std::vector<float>, std::string> ReadField(const Render::RadianceImage &image) {
  auto &device = *image.Device;
  const auto &shape = image.Texture->Description();
  auto transfer = Gpu::RetryOffline(
      [&] {
        return Render::ReadbackTransfer::Start(
            device, *image.Texture, {.Width = shape.Width, .Height = shape.Height});
      },
      kOfflineBudget);
  if (!transfer) { return std::unexpected(transfer.error().Message); }
  const auto pixels = transfer->Wait(device, kOfflineBudget);
  if (!pixels) { return std::unexpected(pixels.error().Message); }
  if (!*pixels) { return std::unexpected("source radiance readback exceeded its offline budget"); }
  std::vector<float> result(size_t{shape.Width} * shape.Height * 4);
  if (!SDL_ConvertPixels(static_cast<int>(shape.Width),
                         static_cast<int>(shape.Height),
                         SDL_PIXELFORMAT_RGBA64_FLOAT,
                         (*pixels)->data(),
                         static_cast<int>(transfer->RowBytes()),
                         SDL_PIXELFORMAT_RGBA128_FLOAT,
                         result.data(),
                         static_cast<int>(shape.Width * 16)) ||
      !std::ranges::all_of(result, [](float x) { return std::isfinite(x); })) {
    return std::unexpected("source radiance field is incomplete or nonfinite");
  }
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
  auto began = Clock::now();
  const auto configured = renderer.ConfigureMaterialFields();
  if (!configured) { return std::unexpected(configured.error().Message); }
  result.ConfigureMs = Elapsed(began);
  const bool uploaded = !result.Source;
  if (uploaded) {
    began = Clock::now();
    auto geometry = original != nullptr ? std::optional(original->clone())
                                        : Render::BuildImpostorSurface(atlas, view);
    if (!geometry) { return std::unexpected("source material field unavailable"); }
    auto input = Render::MaterialFieldInput::Prepare(
        std::move(*geometry), {.WidthPx = atlas.Pixels(), .HeightPx = atlas.Pixels()});
    if (!input) { return std::unexpected(input.error()); }
    result.SourcePrepareMs = Elapsed(began);
    began = Clock::now();
    auto source =
        Gpu::RetryOffline([&] { return renderer.PrepareMaterialField(*input); }, kOfflineBudget);
    if (!source) { return std::unexpected(source.error().Message); }
    result.Source = std::move(*source);
    result.SourceUploadMs = Elapsed(began);
  }
  auto placed = ViewOf(atlas, view, query);
  if (!placed) { return std::unexpected(placed.error()); }
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
    auto pixels = ReadField(*images->Unfiltered);
    if (!pixels) { return std::unexpected(pixels.error()); }
    result.SourceLinear = std::move(*pixels);
    result.ReadbackMs = Elapsed(began);
  }
  return {};
}
} // namespace

std::expected<BuildingRadianceField, std::string>
PrepareBuildingRadiance(Render::SceneRenderer &renderer,
                        const ImpostorAtlas &atlas,
                        size_t view,
                        const nlohmann::json &query,
                        const Geometry *original,
                        const std::shared_ptr<const Render::ResidentMaterialField> &source) {
  const auto began = Clock::now();
  BuildingRadianceField result;
  result.Source = source;
  const auto shaded = Shade(renderer, atlas, view, query, result, original);
  if (!shaded) { return std::unexpected(shaded.error()); }
  auto surface = Render::BuildRadianceSupport(atlas, view);
  if (!surface) { return std::unexpected("radiance support unavailable"); }
  result.Cards.Centre = atlas.CentreM();
  result.Cards.HalfExtentM = atlas.HalfExtentM();
  result.Cards.Views.push_back(
      {.Direction = atlas.Views()[view].TowardEye, .Surface = std::move(*surface)});
  result.PrepareMs = Elapsed(began);
  return result;
}
} // namespace outshine::Test
