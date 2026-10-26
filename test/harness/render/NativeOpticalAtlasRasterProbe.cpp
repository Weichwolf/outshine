#include "SceneLighting.h"
#include "Check.h"
#include "ImpostorSurface.h"
#include "RuntimeScene.h"
#include "SceneRenderer.h"
#include "GeometryProductCodec.h"
#include <cmath>

#include <SDL3/SDL.h>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <fstream>
#include <ios>
#include <limits>
#include <memory>
#include <optional>
#include <print>
#include <span>
#include <string>
#include <utility>
#include <vector>

namespace {
using namespace outshine;
using namespace outshine::Test;
constexpr uint32_t kPixels = 64;
constexpr double kNearM = 0.1;
constexpr float kLux = 100;
constexpr double kMipScale = 64;
constexpr size_t kCenter = (size_t{kPixels} * (kPixels / 2) + kPixels / 2) * 4;
constexpr float kRelativeImageBound = 0.01f;
constexpr size_t kProductBytesMost = 65536;
constexpr float kSheetTransmission = 0.2f;

Geometry NativeSheet(float roughness) {
  Geometry mesh;
  Material material;
  material.BaseColour = {{0, 0, 0, 1}};
  material.Roughness = roughness;
  const auto normal = mesh.addImage(1, 1, std::array<uint8_t, 4>{128, 128, 255, 255});
  CHECK(normal.has_value(), "reference normal image is accepted");
  if (!normal) { return mesh; }
  material.NormalMap.Image = *normal;
  const auto surface = mesh.addSurface("black dielectric", material);
  CHECK(surface.has_value(), "native material is accepted");
  if (!surface) { return mesh; }
  const auto partResult = mesh.addPart("sheet", *surface);
  CHECK(partResult.has_value(), "native sheet part is accepted");
  if (!partResult) { return mesh; }
  const auto part = *partResult;
  CHECK(mesh.setPositions(part, std::array<float, 12>{-1, -1, 0, 1, -1, 0, 1, 1, 0, -1, 1, 0}),
        "native sheet positions");
  CHECK(mesh.setNormals(part, std::array<float, 12>{0, 0, 1, 0, 0, 1, 0, 0, 1, 0, 0, 1}),
        "native sheet normals");
  CHECK(
      mesh.setTangents(part, std::array<float, 16>{1, 0, 0, 1, 1, 0, 0, 1, 1, 0, 0, 1, 1, 0, 0, 1}),
      "native sheet tangents");
  CHECK(mesh.setTexture(part, std::array<float, 8>{0, 0, 1, 0, 1, 1, 0, 1}), "native sheet UVs");
  CHECK(mesh.setTriangles(part, std::array<uint32_t, 6>{0, 1, 2, 0, 2, 3}), "native sheet indices");
  return mesh;
}

std::optional<Geometry> OpticalCard(float roughness) {
  Material material;
  material.BaseColour = {{0, 0, 0, 1}};
  material.Roughness = roughness;
  ImpostorAtlas::View view{.TowardEye = {{0, 0, 1}}, .Texels = {}, .Materials = {}};
  view.Texels.assign(16, {.Normal = {{0, 0, 1}}, .Depth = .5f, .Surface = 1});
  std::string error;
  Material sheet;
  sheet.Scattering = ScatteringModel::DiffuseThinSheet;
  sheet.BaseColour = {{0, 0, 0, 1}};
  view.Texels[0].Surface = 2;
  sheet.DiffuseTransmission = {{kSheetTransmission, kSheetTransmission, kSheetTransmission}};
  const auto atlas = ImpostorAtlas::Create(4, {}, 1, {material, sheet}, {view}, error);
  CHECK(atlas.has_value(), "constant glossy input creates an atlas");
  if (!atlas) { return std::nullopt; }
  return Render::BuildImpostorSurface(*atlas, 0, Render::ImpostorSurfaceDetail::Flat);
}

std::vector<float> CaptureSheet(const Geometry &mesh) {
  Core::Declaration declaration;
  declaration.InitialGeometry = &mesh;
  declaration.KeyLux = kLux;
  declaration.DrawsSky = false;
  declaration.SurfaceWidthPx = declaration.SurfaceHeightPx = kPixels;
  declaration.Outputs = {"sceneLinear", "sceneDepth"};
  Render::SceneRenderer renderer;
  std::unique_ptr<Core::RuntimeScene> scene;
  std::string error;
  const bool opened = Core::RuntimeScene::Open(renderer, declaration, nullptr, scene, error);
  CHECK(opened, "gloss fixture opens through the native runtime");
  if (!opened || !scene) {
    std::println(stderr, "open: {}", error);
    return {};
  }
  const bool ready = renderer.Settle(error);
  CHECK(ready, "offline fixture uploads complete");
  if (!ready) { return {}; }
  Render::Viewpoint eye;
  eye.EyeM = {{0, 0, 4}};
  eye.Kind = Render::CameraKind::Orthographic;
  eye.XMagM = eye.YMagM = 1;
  eye.ZNearM = kNearM;
  eye.ZFarM = 10;
  scene->Eye(eye);
  const bool drawn = scene->Draw(error);
  CHECK(drawn, "gloss fixture publishes its view");
  if (!drawn) {
    std::println(stderr, "draw: {}", error);
    return {};
  }
  renderer.SetGeometryEnvironment({});
  Render::SceneLight key;
  key.Light.Kind = LightKind::Directional;
  key.Light.Intensity = kLux;
  key.Light.Direction = {{0, 0, -1}};
  renderer.SetShadowFrame({{0, 0, 1}}, {{0, 1, 0}}, 256, true, std::numeric_limits<size_t>::max());
  const bool lit = renderer.SetGeometryLights(std::span(&key, size_t{1}), error);
  CHECK(lit, "one current directional light is installed");
  if (!lit) { return {}; }
  const bool rendered = renderer.RenderFrame().has_value();
  CHECK(rendered, "gloss fixture renders through the actual material shader");
  if (!rendered) { return {}; }
  renderer.WaitForGpu();
  std::vector<float> result;
  const bool read = renderer.ReadSceneLinear(result) == Render::ReadState::Ready &&
                    result.size() == size_t{kPixels} * kPixels * 4;
  CHECK(read, "full linear raster is available");
  if (!read) { return {}; }
  std::vector<float> depth;
  const bool depthReady = renderer.ReadDepth(depth) == Render::ReadState::Ready &&
                          depth.size() == size_t{kPixels} * kPixels;
  CHECK(depthReady, "full depth raster is available");
  if (!depthReady) { return {}; }
  size_t covered = 0;
  for (const float z : depth) { covered += static_cast<size_t>(z > 0); }
  CHECK(covered == size_t{kPixels} * kPixels, "native sheet covers the full raster");
  std::println("covered={} centerDepth={}",
               covered,
               depth.empty() ? -1.f : depth[depth.size() / 2 + kPixels / 2]);
  return read ? result : std::vector<float>{};
}

void Save(std::span<const float> values, const std::string &path) {
  std::ofstream file(path, std::ios::binary);
  file.write(reinterpret_cast<const char *>(values.data()),
             static_cast<std::streamsize>(values.size_bytes()));
  CHECK(static_cast<bool>(file), "complete linear raster bytes save");
}

void CheckCard(const std::vector<float> &native,
               float roughness,
               const std::string &directory,
               bool coarse) {
  auto card = OpticalCard(roughness);
  CHECK(card.has_value(), "mixed atlas becomes a native card");
  if (!card) { return; }
  auto material = card->surfaceAt(MaterialInstance(0));
  CHECK(material.Scattering == ScatteringModel::AreaMixture,
        "the input uses the optical material-moment path");
  CHECK(card->imageAt(material.MetalRoughMap.Image).Format == ImagePixelFormat::Rgba16Float,
        "the product really owns the half-float material-moment image");
  if (coarse) {
    for (auto *map : {&material.BaseColourMap,
                      &material.NormalMap,
                      &material.MetalRoughMap,
                      &material.DiffuseReflectionMap,
                      &material.DiffuseTransmissionMap}) {
      map->Uv.ScaleUv = {{kMipScale, kMipScale}};
    }
    CHECK(card->setSurface(MaterialInstance(0), material),
          "current UV derivatives select the final mip");
  }
  const auto encoded = EncodeGeometryProduct(*card, kProductBytesMost);
  CHECK(encoded.has_value(), "complete native card encodes without discarding its typed mips");
  if (!encoded) { return; }
  auto restored = DecodeGeometryProduct(*encoded, encoded->size());
  CHECK(restored.has_value(), "typed card loads as a native product before rendering");
  if (!restored) { return; }
  const auto optical = CaptureSheet(*restored);
  if (optical.empty()) { return; }
  const char *mode = coarse ? "mip" : "base";
  const float opaqueWeight = coarse ? 15.0f / 16.0f : 1.0f;
  size_t nonfinite = 0;
  for (const float value : optical) { nonfinite += static_cast<size_t>(!std::isfinite(value)); }
  CHECK(nonfinite == 0, "native card raster contains finite radiance throughout");
  CHECK(native[kCenter] > 0 && optical[kCenter] > 0,
        "constant glossy highlights survive the optical atlas");
  const float expected = native[kCenter] * opaqueWeight;
  CHECK(std::abs(optical[kCenter] - expected) <= expected * kRelativeImageBound,
        "current optical highlight stays within the declared one-percent image precision bound");
  std::println("mode={} r={} native={} card={} expected={} relative={}",
               mode,
               roughness,
               native[kCenter],
               optical[kCenter],
               expected,
               expected > 0 ? std::abs(optical[kCenter] - expected) / expected : 0);
  const auto stem = directory + "/atlas-" + mode + "-" + std::to_string(roughness);
  Save(optical, stem + ".f32");
  std::ofstream product(stem + ".asset", std::ios::binary);
  product.write(reinterpret_cast<const char *>(encoded->data()),
                static_cast<std::streamsize>(encoded->size()));
  CHECK(static_cast<bool>(product), "the exact rendered native product saves for reproduction");
}
}

int main(int argc, char *const *argv) {
  if (argc != 2) { return 2; }
  const bool initialized = SDL_Init(SDL_INIT_VIDEO);
  CHECK(initialized, "video starts for the native optical raster fixture");
  if (!initialized) { return Report(); }
  for (const float roughness : {.12f, .2f, .5f}) {
    const auto native = CaptureSheet(NativeSheet(roughness));
    if (native.empty()) { continue; }
    Save(native, std::string(argv[1]) + "/native-" + std::to_string(roughness) + ".f32");
    CheckCard(native, roughness, argv[1], false);
    CheckCard(native, roughness, argv[1], true);
  }
  SDL_Quit();
  return Report();
}
