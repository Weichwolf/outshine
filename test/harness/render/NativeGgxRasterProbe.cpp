#include "Check.h"
#include "RuntimeScene.h"
#include "SceneRenderer.h"

#include <SDL3/SDL.h>
#include <array>
#include <cstddef>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <fstream>
#include <ios>
#include <limits>
#include <memory>
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
constexpr float kUnderlyingRoughness = 0.5f;
enum class Finish { Base, Clearcoat, Iridescent };
constexpr std::array<std::pair<Finish, const char *>, 3> kFinishes{
    {{Finish::Base, "base"}, {Finish::Clearcoat, "clearcoat"}, {Finish::Iridescent, "iridescent"}}};

Geometry NativeSheet(float roughness, Finish finish) {
  Geometry mesh;
  Material material;
  material.BaseColour = {{0, 0, 0, 1}};
  material.Roughness = roughness;
  if (finish == Finish::Clearcoat) {
    material.Ior = 0;
    material.Roughness = kUnderlyingRoughness;
    material.Clearcoat = 1;
    material.ClearcoatRoughness = roughness;
  }
  if (finish == Finish::Iridescent) { material.Iridescence = 1; }
  const auto surface = mesh.addSurface("black dielectric", material);
  CHECK(surface.has_value(), "fixture material is accepted");
  if (!surface) { return mesh; }
  const auto part = mesh.addPart("sheet", *surface);
  CHECK(part.has_value(), "fixture part is accepted");
  if (!part) { return mesh; }
  CHECK(mesh.setPositions(*part, std::array<float, 12>{-1, -1, 0, 1, -1, 0, 1, 1, 0, -1, 1, 0}),
        "native sheet positions");
  CHECK(mesh.setNormals(*part, std::array<float, 12>{0, 0, 1, 0, 0, 1, 0, 0, 1, 0, 0, 1}),
        "native sheet normals");
  CHECK(mesh.setTangents(*part,
                         std::array<float, 16>{1, 0, 0, 1, 1, 0, 0, 1, 1, 0, 0, 1, 1, 0, 0, 1}),
        "native sheet tangents");
  CHECK(mesh.setTexture(*part, std::array<float, 8>{0, 0, 1, 0, 1, 1, 0, 1}), "native sheet UVs");
  CHECK(mesh.setTriangles(*part, std::array<uint32_t, 6>{0, 1, 2, 0, 2, 3}),
        "native sheet indices");
  return mesh;
}

std::vector<float> CaptureSheet(const Geometry &mesh, float lux) {
  Core::Declaration declaration;
  declaration.InitialGeometry = &mesh;
  declaration.KeyLux = lux;
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
  Render::SubjectLight key;
  key.Light.Kind = LightKind::Directional;
  key.Light.Intensity = lux;
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
} // namespace

int main(int argc, char *const *argv) {
  if (argc != 2) { return 2; }
  const bool initialized = SDL_Init(SDL_INIT_VIDEO);
  CHECK(initialized, "video starts for GGX raster controls");
  if (!initialized) { return Report(); }
  for (const auto &[finish, label] : kFinishes) {
    for (const float roughness : {.01f, .02f, .12f, .5f}) {
      const float alpha = roughness * roughness;
      const float lux = 100.0f * alpha * alpha;
      const auto values = CaptureSheet(NativeSheet(roughness, finish), lux);
      if (values.empty()) { continue; }
      const size_t center = (size_t{kPixels} * (kPixels / 2) + kPixels / 2) * 4;
      size_t nonfinite = 0;
      for (const float value : values) { nonfinite += static_cast<size_t>(!std::isfinite(value)); }
      std::println("finish={} r={} lux={} center=({},{},{}) nonfinite={}",
                   label,
                   roughness,
                   lux,
                   values[center],
                   values[center + 1],
                   values[center + 2],
                   nonfinite);
      CHECK(nonfinite == 0, "representable GGX raster has finite linear radiance");
      CHECK(values[center] > 0, "the direct specular lobe remains present");
      const auto path =
          std::string(argv[1]) + "/" + label + "-r-" + std::to_string(roughness) + ".f32";
      std::ofstream output(path, std::ios::binary);
      output.write(reinterpret_cast<const char *>(values.data()),
                   static_cast<std::streamsize>(values.size() * sizeof(float)));
      CHECK(static_cast<bool>(output), "native raster bytes save");
    }
  }
  SDL_Quit();
  return Report();
}
