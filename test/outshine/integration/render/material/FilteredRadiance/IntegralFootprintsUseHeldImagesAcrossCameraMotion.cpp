#include "Check.h"
#include "ImpostorInstances.h"
#include "RadianceSupport.h"
#include "MaterialFieldPass.h"
#include "RuntimeScene.h"
#include "SceneRenderer.h"
#include "test/harness/render/NativeGpuCheck.h"
#include "test/harness/render/NativeOffline.h"
#include <SDL3/SDL.h>
#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <span>
#include <string>
#include <utility>
#include <vector>

namespace {
using namespace outshine;
using namespace outshine::Render;
using namespace outshine::Test;
using namespace outshine::Test::Gpu;
using Colours = std::array<std::array<float, 3>, 4>;
constexpr Colours kColours{
    {{.125f, .25f, .5f}, {.5f, .125f, .25f}, {.25f, .5f, .125f}, {.75f, .5f, .25f}}};

void Quad(Geometry &into, std::array<float, 4> bounds, Vec4f colour, bool blended) {
  Material row;
  row.BaseColour = colour;
  row.Unlit = true;
  if (colour[0] >= 1) {
    row.BaseColour = {{0, 0, 0, 1}};
    row.Emission = {{colour[0], colour[1], colour[2]}};
    row.Unlit = false;
  }
  row.DoubleSided = true;
  row.Alpha = blended ? AlphaMode::Blended : AlphaMode::Opaque;
  const auto material = into.addSurface("field", row);
  CHECK(material, "the independent coloured field has a material");
  if (!material) { Failed(); }
  const auto part = into.addPart("quad", *material);
  CHECK(part, "the field has a geometry part");
  if (!part) { Failed(); }
  const auto [left, bottom, right, top] = bounds;
  CHECK(into.setPositions(*part,
                          std::array<float, 12>{
                              left, bottom, 0, right, bottom, 0, right, top, 0, left, top, 0}) &&
            into.setNormals(*part, std::array<float, 12>{0, 0, 1, 0, 0, 1, 0, 0, 1, 0, 0, 1}) &&
            into.setTexture(*part,
                            std::array<float, 8>{.5f + left * .5f,
                                                 .5f - bottom * .5f,
                                                 .5f + right * .5f,
                                                 .5f - bottom * .5f,
                                                 .5f + right * .5f,
                                                 .5f - top * .5f,
                                                 .5f + left * .5f,
                                                 .5f - top * .5f}) &&
            into.setTriangles(*part, std::array<uint32_t, 6>{0, 1, 2, 0, 2, 3}),
        "the source and expanded support carry coherent world coordinates and UVs");
}

Geometry Source(const Colours &colours) {
  Geometry source;
  for (size_t at = 0; at < colours.size(); ++at) {
    const float left = static_cast<float>(at % 2) - 1;
    const size_t row = at / 2;
    const float bottom = static_cast<float>(row) - 1;
    const auto &colour = colours[at];
    Quad(source,
         {left, bottom, left + 1, bottom + 1},
         {{colour[0], colour[1], colour[2], 1}},
         false);
  }
  return source;
}

std::array<double, 4> Average(std::array<double, 4> pixel, const Colours &colours) {
  std::array<double, 4> result{};
  const double area = (pixel[2] - pixel[0]) * (pixel[3] - pixel[1]);
  for (size_t at = 0; at < colours.size(); ++at) {
    const double left = static_cast<double>(at % 2) - 1;
    const size_t row = at / 2;
    const double bottom = static_cast<double>(row) - 1;
    const double coverage =
        std::max(0.0, std::min(pixel[2], left + 1) - std::max(pixel[0], left)) *
        std::max(0.0, std::min(pixel[3], bottom + 1) - std::max(pixel[1], bottom)) / area;
    for (size_t channel = 0; channel < 3; ++channel) {
      result[channel] += coverage * colours[at][channel];
    }
    result[3] += coverage;
  }
  return result;
}

void CheckFrame(Core::RuntimeScene &scene,
                SceneRenderer &renderer,
                const std::shared_ptr<const RadianceImage> &image,
                std::array<double, 2> offset,
                const Colours &colours) {
  Viewpoint camera;
  camera.Kind = CameraKind::Orthographic;
  camera.EyeM = {{offset[0], offset[1], 4}};
  camera.XMagM = camera.YMagM = 4;
  camera.ZNearM = .1;
  camera.ZFarM = 10;
  scene.Eye(camera);
  const auto lens = Lens::From(camera, 32, 32);
  CHECK(lens, "the exact frame lens is available for projected support");
  if (!lens) { Failed(); }
  auto support = BuildProjectedRadianceSupport(
      {.Centre = {}, .TowardEye = {{0, 0, 1}}, .HalfExtentM = 1}, camera, *lens);
  CHECK(support, "the support projects to a pixel-aligned rectangle");
  if (!support) { Failed(); }
  ImpostorCards cards;
  cards.HalfExtentM = 1;
  cards.Views.push_back({.Direction = {{0, 0, 1}}, .Surface = std::move(*support)});
  const std::array images{image};
  std::string error;
  const auto instances = ImpostorInstances::Create(renderer, cards, 1, error, images);
  CHECK(instances, "the existing native instance path binds the integral material");
  if (!instances) { Failed(); }
  const Mat4 model;
  CHECK(instances->Update(std::span(&model, 1), camera.EyeM, error),
        "the held image has one stable instance");
  CHECK(scene.Draw(error),
        "normal frames consume the already submitted integral without rebuilding the scene");
  renderer.WaitForGpu();
  std::vector<float> pixels;
  CHECK(renderer.ReadSceneLinear(pixels) == ReadState::Ready &&
            pixels.size() == size_t{32} * 32 * 4,
        "the independent frame has its expected linear HDR raster");
  if (pixels.size() != size_t{32} * 32 * 4) { Failed(); }
  for (size_t y = 0; y < 32; ++y) {
    for (size_t x = 0; x < 32; ++x) {
      const double left = offset[0] + (static_cast<double>(x) - 16) * .25;
      const double top = offset[1] + (16 - static_cast<double>(y)) * .25;
      const auto expected = Average({left, top - .25, left + .25, top}, colours);
      for (size_t channel = 0; channel < 4; ++channel) {
        if (channel == 0 && expected[0] > 1) { continue; }
        const auto actual = pixels[(y * 32 + x) * 4 + channel];
        CHECK(std::isfinite(actual) && std::abs(actual - expected[channel]) < .001,
              "direct prefix sampling matches independent world-space overlap, including support "
              "edges");
      }
    }
  }
}

void Exercise(const Colours &colours, int raster) {
  SceneRenderer renderer;
  Core::Declaration declaration;
  declaration.SurfaceWidthPx = declaration.SurfaceHeightPx = 32;
  declaration.Exposure = 1;
  declaration.Haze = 0;
  declaration.Stages = {"opaqueGeometry", "transmissiveGeometry", "compositeTransmission"};
  declaration.Outputs = {"sceneLinear"};
  std::unique_ptr<Core::RuntimeScene> scene;
  std::string error;
  CHECK(Core::RuntimeScene::Open(renderer, declaration, nullptr, scene, error),
        "the ordinary render scene opens once");
  if (!scene) { Failed(); }
  Require(renderer.ConfigureMaterialFields());
  const auto input =
      MaterialFieldInput::Prepare(Source(colours), {.WidthPx = raster, .HeightPx = raster});
  CHECK(input, "the unlit source contains all four original colour regions");
  if (!input) { Failed(); }
  const auto source = Require(
      RetryOffline([&] { return renderer.PrepareMaterialField(*input); }, std::chrono::seconds(5)));
  MaterialFieldView view;
  view.Camera.Kind = CameraKind::Orthographic;
  view.Camera.EyeM = {{0, 0, 2}};
  view.Camera.XMagM = view.Camera.YMagM = 1;
  view.Camera.ZNearM = .1;
  view.Camera.ZFarM = 10;
  view.Target = {.WidthPx = raster, .HeightPx = raster};
  view.Encoding = RadianceEncoding::IntegralPairs;
  const auto images = Require(RetryOffline(
      [&] { return renderer.RelightMaterialField(*source, view); }, std::chrono::seconds(5)));
  for (const auto offset :
       std::array<std::array<double, 2>, 3>{{{.02, .03}, {.13, .11}, {-.13, -.11}}}) {
    CheckFrame(*scene, renderer, images.Filtered, offset, colours);
  }
}
}

int main() {
  CHECK(SDL_Init(SDL_INIT_VIDEO), "video initializes the real frame consumer");
  Exercise(kColours, 32);
  auto hdr = kColours;
  hdr[0][0] = 4096;
  Exercise(hdr, 128);
  hdr[0][0] = 1;
  Exercise(hdr, 128);
  SDL_Quit();
  return Report();
}
