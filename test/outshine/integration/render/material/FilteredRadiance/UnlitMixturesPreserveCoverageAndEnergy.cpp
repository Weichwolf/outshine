#include "Check.h"
#include "Geometry.h"
#include "HalfTexels.h"
#include "RuntimeScene.h"
#include "SceneRenderer.h"
#include <SDL3/SDL.h>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <initializer_list>
#include <string>
#include <vector>

namespace {
using namespace outshine;
using namespace outshine::Test;

Geometry Surface(float coverage, ScatteringModel scattering) {
  Geometry mesh;
  Material material;
  material.Unlit = true;
  material.Alpha = AlphaMode::Blended;
  material.Scattering = scattering;
  material.BaseColour = {{1, 1, 1, 1}};
  const float weight = scattering == ScatteringModel::AreaMixture ? coverage : 1;
  const auto pixels =
      Core::PackHalfTexels({.WidthPx = 1, .HeightPx = 1},
                           std::array<float, 4>{2 * weight, weight, .5f * weight, coverage});
  CHECK(pixels, "finite premultiplied HDR field packs without clipping");
  if (!pixels) { return mesh; }
  const auto image = mesh.addImage(
      {.WidthPx = 1, .HeightPx = 1, .Format = ImagePixelFormat::Rgba16Float, .Rgba = *pixels});
  CHECK(image, "linear HDR field is a supported native image");
  if (!image) { return mesh; }
  material.BaseColourMap.Image = *image;
  if (scattering == ScatteringModel::AreaMixture) {
    const auto empty = mesh.addImage(1, 1, std::array<uint8_t, 4>{0, 0, 0, 0});
    CHECK(empty, "absent thin-sheet contribution has an explicit zero field");
    if (!empty) { return mesh; }
    material.DiffuseReflectionMap.Image = material.DiffuseTransmissionMap.Image = *empty;
  }
  const auto surface = mesh.addSurface("field", material);
  CHECK(surface, "optical and ordinary unlit material contracts remain valid");
  if (!surface) { return mesh; }
  const auto part = mesh.addPart("support", *surface);
  CHECK(part, "radiance support is a native geometry part");
  if (!part) { return mesh; }
  CHECK(mesh.setPositions(*part, std::array<float, 12>{-1, -1, 0, 1, -1, 0, 1, 1, 0, -1, 1, 0}),
        "support positions");
  CHECK(mesh.setTexture(*part, std::array<float, 8>{0, 1, 1, 1, 1, 0, 0, 0}), "support UVs");
  CHECK(mesh.setTriangles(*part, std::array<uint32_t, 6>{0, 1, 2, 0, 2, 3}), "support triangles");
  return mesh;
}

void Exercise(float coverage, ScatteringModel scattering) {
  const auto mesh = Surface(coverage, scattering);
  if (!mesh.wellFormed()) { return; }
  Render::SceneRenderer renderer;
  Core::Declaration declaration;
  declaration.InitialGeometry = &mesh;
  declaration.SurfaceWidthPx = declaration.SurfaceHeightPx = 32;
  declaration.Exposure = 1;
  declaration.Haze = 0;
  declaration.Stages = {"opaqueGeometry", "transmissiveGeometry", "compositeTransmission"};
  declaration.Outputs = {"sceneLinear"};
  std::unique_ptr<Core::RuntimeScene> scene;
  std::string error;
  CHECK(Core::RuntimeScene::Open(renderer, declaration, nullptr, scene, error),
        "field world opens");
  if (!scene) { return; }
  Render::Viewpoint eye;
  eye.EyeM = {{0, 0, 4}};
  eye.Kind = Render::CameraKind::Orthographic;
  eye.XMagM = eye.YMagM = 2;
  eye.ZNearM = .1;
  eye.ZFarM = 10;
  scene->Eye(eye);
  CHECK(scene->Draw(error), "field world renders through native blending");
  renderer.WaitForGpu();
  std::vector<float> pixels;
  CHECK(renderer.ReadSceneLinear(pixels) == Render::ReadState::Ready &&
            pixels.size() == size_t{32} * 32 * 4,
        "native linear radiance is readable");
  if (pixels.size() != size_t{32} * 32 * 4) { return; }
  const size_t center = (size_t{16} * 32 + 16) * 4;
  const std::array expected{2 * coverage, coverage, .5f * coverage, coverage};
  for (size_t channel = 0; channel < expected.size(); ++channel) {
    CHECK(std::isfinite(pixels[center + channel]) &&
              std::abs(pixels[center + channel] - expected[channel]) < .002f,
          "coverage multiplies radiance exactly once, including empty/full/HDR endpoints");
  }
}
}

int main() {
  using namespace outshine;
  using namespace outshine::Test;
  CHECK(SDL_Init(SDL_INIT_VIDEO), "video initializes for native optical fields");
  if (SDL_WasInit(SDL_INIT_VIDEO) != 0) {
    for (const float coverage : {0.0f, .25f, .5f, 1.0f}) {
      Exercise(coverage, ScatteringModel::AreaMixture);
      Exercise(coverage, ScatteringModel::MetallicRoughness);
    }
  }
  SDL_Quit();
  return outshine::Test::Report();
}
