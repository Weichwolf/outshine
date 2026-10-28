#include "BuildingRadianceField.h"
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

std::expected<void, std::string> Shade(Render::SceneRenderer &renderer,
                                       const ImpostorAtlas &atlas,
                                       size_t view,
                                       const nlohmann::json &query,
                                       BuildingRadianceField &result,
                                       const Geometry *original) {
  auto source = original != nullptr ? std::optional(original->clone())
                                    : Render::BuildImpostorSurface(atlas, view);
  if (!source) { return std::unexpected("source material field unavailable"); }
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
  if (query.value("dump_source_field", true) &&
      (renderer.ReadSceneLinear(result.SourceLinear) != Render::ReadState::Ready ||
       result.SourceLinear.size() != static_cast<size_t>(atlas.Pixels()) * atlas.Pixels() * 4 ||
       !std::ranges::all_of(result.SourceLinear, [](float x) { return std::isfinite(x); }))) {
    return std::unexpected("source radiance field is incomplete or nonfinite");
  }
  for (const auto &sample : renderer.GpuSamples()) { result.GpuMs += sample.ElapsedMs; }
  const double radius = std::numbers::sqrt3 * atlas.HalfExtentM();
  const double diameterPx = 2 * radius * query.at("focal_px").get<double>() /
                            std::max(1.0, Length(eye - atlas.CentreM()) - radius);
  int pixels = atlas.Pixels();
  while (pixels > 1) {
    const int next = pixels / 2;
    if (static_cast<double>(next) < diameterPx) { break; }
    pixels = next;
  }
  auto filtered = renderer.FilterSceneRadiance({.WidthPx = pixels, .HeightPx = pixels});
  if (!filtered) { return std::unexpected(filtered.error()); }
  result.Radiance = std::move(*filtered);
  return {};
}
}

std::expected<BuildingRadianceField, std::string>
PrepareBuildingRadiance(Render::SceneRenderer &renderer,
                        const ImpostorAtlas &atlas,
                        size_t view,
                        const nlohmann::json &query,
                        const Geometry *original) {
  const auto began = Clock::now();
  BuildingRadianceField result;
  const auto shaded = Shade(renderer, atlas, view, query, result, original);
  if (!shaded) { return std::unexpected(shaded.error()); }
  auto surface = Render::BuildRadianceSupport(atlas, view);
  if (!surface) { return std::unexpected("radiance support unavailable"); }
  result.Cards.Centre = atlas.CentreM();
  result.Cards.HalfExtentM = atlas.HalfExtentM();
  result.Cards.Views.push_back(
      {.Direction = atlas.Views()[view].TowardEye, .Surface = std::move(*surface)});
  result.PrepareMs = std::chrono::duration<double, std::milli>(Clock::now() - began).count();
  return result;
}
}
