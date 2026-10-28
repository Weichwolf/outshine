#include "BuildingRadianceField.h"
#include "HalfTexels.h"
#include "ImageMips.h"
#include "TexelChain.h"
#include "ImpostorSurface.h"
#include "ImpostorViewFrame.h"
#include "RuntimeScene.h"
#include "SceneRenderer.h"
#include <nlohmann/json.hpp>
#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstddef>
#include <expected>
#include <string>
#include <memory>
#include <ratio>
#include <numbers>
#include <optional>
#include <span>
#include <utility>
#include <vector>

namespace outshine::Test {
namespace {
using Clock = std::chrono::steady_clock;

std::expected<std::vector<float>, std::string> Shade(const ImpostorAtlas &atlas,
                                                     size_t view,
                                                     const nlohmann::json &query,
                                                     BuildingRadianceField &result,
                                                     const Geometry *original) {
  auto source = original != nullptr ? std::optional(original->clone())
                                    : Render::BuildImpostorSurface(atlas, view);
  if (!source) { return std::unexpected("source material field unavailable"); }
  Render::GpuSubmission submission;
  submission.Create = [](void *, Render::Native::DeviceOptions options) {
    options.GpuTiming = Render::Native::GpuTimingMode::Submission;
    return Render::Native::CreateDevice(options);
  };
  Render::SceneRenderer renderer{submission};
  Core::Declaration declaration;
  declaration.InitialGeometry = &*source;
  declaration.SurfaceWidthPx = declaration.SurfaceHeightPx = atlas.Pixels();
  declaration.KeyLux = 100;
  declaration.KeyElevationDeg = query.value("sun_elevation_deg", 0.0);
  declaration.Haze = 0;
  declaration.Exposure = 1;
  declaration.Stages = {"opaqueGeometry"};
  declaration.Outputs = {"sceneLinear"};
  std::unique_ptr<Core::RuntimeScene> scene;
  std::string error;
  if (!Core::RuntimeScene::Open(renderer, declaration, nullptr, scene, error)) {
    return std::unexpected(error);
  }
  const Vec3 direction = atlas.Views()[view].TowardEye;
  const auto frame = ImpostorViewFrame::From(direction);
  if (!frame) { return std::unexpected("source view frame is singular"); }
  const auto eyeValues = query.at("eye").get<std::array<double, 3>>();
  const Vec3 eye{eyeValues};
  Render::Viewpoint camera;
  camera.EyeM = atlas.CentreM() + direction * Length(eye - atlas.CentreM());
  camera.Right = frame->Right;
  camera.Up = frame->Up;
  camera.Forward = direction * -1.0;
  camera.Kind = Render::CameraKind::Orthographic;
  camera.XMagM = camera.YMagM = atlas.HalfExtentM();
  camera.ZNearM = .1;
  camera.ZFarM = Length(camera.EyeM - atlas.CentreM()) + 4 * atlas.HalfExtentM();
  renderer.ResetMeasurements();
  renderer.KeepGpuSamples(1);
  scene->Eye(camera);
  if (!scene->Draw(error)) { return std::unexpected(error); }
  renderer.WaitForGpu();
  for (const auto &sample : renderer.GpuSamples()) { result.SetupGpuMs += sample.ElapsedMs; }
  renderer.ResetMeasurements();
  renderer.KeepGpuSamples(1);
  if (const auto rendered = renderer.RenderFrame(); !rendered) {
    return std::unexpected(rendered.error());
  }
  renderer.WaitForGpu();
  std::vector<float> values;
  if (renderer.ReadSceneLinear(values) != Render::ReadState::Ready ||
      values.size() != static_cast<size_t>(atlas.Pixels()) * atlas.Pixels() * 4 ||
      !std::ranges::all_of(values, [](float x) { return std::isfinite(x); })) {
    return std::unexpected("source radiance field is incomplete or nonfinite");
  }
  for (const auto &sample : renderer.GpuSamples()) { result.GpuMs += sample.ElapsedMs; }
  return values;
}

std::expected<int, std::string>
AddRadiance(Geometry &geometry, std::vector<float> values, Core::Texels extent, double diameterPx) {
  while (extent.WidthPx > 1) {
    const uint32_t next = extent.WidthPx / 2;
    if (static_cast<double>(next) < diameterPx) { break; }
    std::vector<float> filtered;
    extent = Core::HalveInPlace(values, extent, filtered, Core::TexelKind::Value);
    values = std::move(filtered);
  }
  const uint32_t pixels = extent.WidthPx;
  auto base = Core::PackHalfTexels(extent, values);
  if (!base) { return std::unexpected("invalid radiance base"); }
  ImageMipData mips;
  auto &lower = mips[static_cast<size_t>(ImageMipKind::Colour)].emplace();
  while (extent.WidthPx > 1 || extent.HeightPx > 1) {
    std::vector<float> filtered;
    extent = Core::HalveInPlace(values, extent, filtered, Core::TexelKind::Value);
    values = std::move(filtered);
    const auto bytes = Core::PackHalfTexels(extent, values);
    if (!bytes) { return std::unexpected("invalid radiance level"); }
    lower.insert(lower.end(), bytes->begin(), bytes->end());
  }
  const auto image = geometry.addImage({.WidthPx = static_cast<int>(pixels),
                                        .HeightPx = static_cast<int>(pixels),
                                        .Format = ImagePixelFormat::Rgba16Float,
                                        .Rgba = *base,
                                        .LowerMips = ViewImageMips(mips)});
  if (!image) { return std::unexpected("radiance image rejected"); }
  return *image;
}
}

std::expected<BuildingRadianceField, std::string>
PrepareBuildingRadiance(const ImpostorAtlas &atlas,
                        size_t view,
                        const nlohmann::json &query,
                        const Geometry *original) {
  const auto began = Clock::now();
  BuildingRadianceField result;
  auto values = Shade(atlas, view, query, result, original);
  if (!values) { return std::unexpected(values.error()); }
  result.SourceLinear = *values;
  auto surface = Render::BuildFractionalImpostorSurface(atlas, view);
  if (!surface) { return std::unexpected("radiance support unavailable"); }
  const Vec3 eye{query.at("eye").get<std::array<double, 3>>()};
  const double radius = std::numbers::sqrt3 * atlas.HalfExtentM();
  const double diameterPx = 2 * radius * query.at("focal_px").get<double>() /
                            std::max(1.0, Length(eye - atlas.CentreM()) - radius);
  const auto image = AddRadiance(*surface,
                                 std::move(*values),
                                 {.WidthPx = static_cast<uint32_t>(atlas.Pixels()),
                                  .HeightPx = static_cast<uint32_t>(atlas.Pixels())},
                                 diameterPx);
  if (!image) { return std::unexpected(image.error()); }
  const auto id = surface->materialOf(0);
  auto material = surface->surfaceAt(id);
  material.Unlit = true;
  material.BaseColourMap.Image = *image;
  material.BaseColourMap.Sampler.Minify = material.BaseColourMap.Sampler.Magnify = Filter::Nearest;
  material.BaseColourMap.Sampler.Mip = MipFilter::None;
  material.NormalMap = {};
  material.MetalRoughMap = {};
  if (!surface->setSurface(id, material)) { return std::unexpected("radiance material rejected"); }
  result.Cards.Centre = atlas.CentreM();
  result.Cards.HalfExtentM = atlas.HalfExtentM();
  result.Cards.Views.push_back(
      {.Direction = atlas.Views()[view].TowardEye, .Surface = std::move(*surface)});
  result.PrepareMs = std::chrono::duration<double, std::milli>(Clock::now() - began).count();
  return result;
}
}
