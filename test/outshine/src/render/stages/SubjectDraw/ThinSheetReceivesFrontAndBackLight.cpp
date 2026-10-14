#include "Check.h"
#include "RuntimeScene.h"
#include "SceneRenderer.h"
#include "math/Srgb.h"

#include <SDL3/SDL.h>
#include <algorithm>
#include <array>
#include <cmath>
#include <memory>
#include <numbers>
#include <print>
#include <string>
#include <vector>

namespace {
outshine::Geometry SheetGeometry(outshine::Material material, bool mapped) {
  using namespace outshine;
  using namespace outshine::Render;
  using namespace outshine::Test;
  Geometry geometry;
  if (mapped) {
    constexpr std::array<uint8_t, 8> pixels{255, 255, 255, 255, 255, 128, 255, 255};
    const auto image = geometry.addImage(2, 1, pixels);
    CHECK(image.has_value(), "the mapped sheet owns two independently identifiable texels");
    if (!image) { return {}; }
    material.BaseColourMap.Image = *image;
    material.BaseColourMap.Set = UvSet::Uv1;
    material.BaseColourMap.Sampler.Magnify = Filter::Nearest;
    material.BaseColourMap.Sampler.Minify = Filter::Nearest;
    material.BaseColourMap.Sampler.Mip = MipFilter::None;
  }
  const auto surface = geometry.addSurface("thin diffuse leaf", material);
  CHECK(surface.has_value(), "energy-bounded direct leaf R/T is a native material");
  if (!surface) { return {}; }
  const auto part = geometry.addPart("single sheet", *surface);
  CHECK(part.has_value(), "one geometric sheet owns the scattering material");
  if (!part) { return {}; }
  constexpr std::array<float, 12> positions{-2, 0, -2, 2, 0, -2, 2, 0, 2, -2, 0, 2};
  constexpr std::array<float, 12> normals{0, 1, 0, 0, 1, 0, 0, 1, 0, 0, 1, 0};
  CHECK(geometry.setPositions(*part, positions) && geometry.setNormals(*part, normals) &&
            geometry.setTriangles(*part, std::array<uint32_t, 6>{0, 2, 1, 0, 3, 2}),
        "sheet geometry has an independent analytical normal");
  if (mapped) {
    constexpr std::array<float, 8> uv0{.25f, .5f, .25f, .5f, .25f, .5f, .25f, .5f};
    constexpr std::array<float, 8> uv1{.75f, .5f, .75f, .5f, .75f, .5f, .75f, .5f};
    CHECK(geometry.setTexture(*part, uv0) && geometry.setTexture(*part, uv1, 1),
          "the mapped sheet distinguishes the two packed UV selectors");
  }
  return geometry;
}

outshine::Vec3 LightResponse(const outshine::Geometry &geometry, double elevation) {
  using namespace outshine;
  using namespace outshine::Render;
  using namespace outshine::Test;
  Core::Declaration declaration;
  declaration.InitialGeometry = &geometry;
  declaration.SurfaceWidthPx = declaration.SurfaceHeightPx = 64;
  declaration.Outputs = {"surface", "sceneLinear"};
  declaration.KeyLux = 1000;
  declaration.KeyElevationDeg = elevation;
  SceneRenderer renderer;
  std::unique_ptr<Core::RuntimeScene> scene;
  std::string error;
  CHECK(Core::RuntimeScene::Open(renderer, declaration, nullptr, scene, error),
        "native thin-sheet scene opens without environment or specular light");
  if (!scene) { return {}; }
  auto eye = Viewpoint::LookAt({.EyeM = {{0, 5, 4}}, .AimM = {{0, 0, 0}}}, 0.0);
  CHECK(eye.has_value(), "sheet camera remains above the same front face");
  if (!eye) { return {}; }
  eye->Kind = CameraKind::Orthographic;
  eye->XMagM = eye->YMagM = 1;
  eye->ZNearM = .1;
  eye->ZFarM = 100;
  scene->Eye(*eye);
  renderer.CastsBelow(0);
  CHECK(scene->Draw(error), "current light reaches the thin-sheet shader");
  renderer.WaitForGpu();
  std::vector<float> pixels;
  CHECK(renderer.ReadSceneLinear(pixels) == ReadState::Ready && pixels.size() == 64u * 64u * 4u,
        "linear GPU radiance is readable without tone mapping");
  if (pixels.size() != 64u * 64u * 4u) { return {}; }
  constexpr size_t centre = (32u * 64u + 32u) * 4u;
  return {{pixels[centre], pixels[centre + 1], pixels[centre + 2]}};
}

outshine::Geometry MixtureGeometry(float coverage) {
  using namespace outshine;
  using namespace outshine::Test;
  Material material;
  material.Scattering = ScatteringModel::AreaMixture;
  material.BaseColour = {{.3f, .14f, .05f, 1}};
  material.DiffuseTransmission = {{1, 1, 1}};
  material.Roughness = 1;
  material.Alpha = AlphaMode::Masked;
  material.DoubleSided = true;
  auto geometry = SheetGeometry(Material{}, true);
  const auto encode = [](double value) { return static_cast<uint8_t>(std::lround(value * 255)); };
  const auto image = [&](const std::array<uint8_t, 4> &pixels, SurfaceMap &map) {
    const auto held = geometry.addImage(1, 1, pixels);
    CHECK(held.has_value(), "the mixture owns an independent optical coefficient map");
    if (held) { map.Image = *held; }
    map.Sampler.Magnify = map.Sampler.Minify = Filter::Nearest;
    map.Sampler.Mip = MipFilter::None;
  };
  const auto mr = encode(ColourSpace::SrgbFromLinear(.5f * coverage));
  image({mr, mr, mr, encode(coverage)}, material.BaseColourMap);
  image({encode(.05 * coverage),
         encode(.1 * coverage),
         encode(.015 * coverage),
         encode(.5 * coverage)},
        material.DiffuseReflectionMap);
  image({encode(.2 * coverage), encode(.3 * coverage), encode(.05 * coverage), 255},
        material.DiffuseTransmissionMap);
  image({0, encode(.35 * coverage), 0, encode(.5 * coverage)}, material.MetalRoughMap);
  CHECK(geometry.setSurface(MaterialInstance(0), material),
        "a mixed surface publishes all covered-area factors");
  return geometry;
}

void CheckMixture() {
  using namespace outshine;
  using namespace outshine::Test;
  Material wood;
  wood.BaseColour = {{.3f, .14f, .05f, 1}};
  wood.Roughness = .7f;
  wood.DoubleSided = true;
  Material leaf;
  leaf.Scattering = ScatteringModel::DiffuseThinSheet;
  leaf.BaseColour = {{.1f, .2f, .03f, 1}};
  leaf.DiffuseTransmission = {{.4f, .6f, .1f}};
  leaf.DoubleSided = true;
  const auto bark = SheetGeometry(wood, false);
  const auto sheet = SheetGeometry(leaf, false);
  for (const double elevation : {45.0, -45.0}) {
    const auto a = LightResponse(bark, elevation);
    const auto b = LightResponse(sheet, elevation);
    for (const float coverage : {1.0f, .75f}) {
      const auto geometry = MixtureGeometry(coverage);
      const auto combined = LightResponse(geometry, elevation);
      for (size_t channel = 0; channel < 3; ++channel) {
        CHECK_NEAR(combined[channel],
                   .5 * (a[channel] + b[channel]),
                   2.0,
                   "linear radiance",
                   "covered-area mixture matches independent runtime wood and leaf responses");
      }
      std::println("MIXTURE elevation={} coverage={} RGB={:.6f},{:.6f},{:.6f}",
                   elevation,
                   coverage,
                   combined[0],
                   combined[1],
                   combined[2]);
    }
  }
}
}

int main() {
  using namespace outshine::Test;
  CHECK(SDL_Init(SDL_INIT_VIDEO), "video initializes for native front/back leaf light");
  {
    constexpr std::array<double, 3> reflection{.1, .2, .03};
    constexpr std::array<double, 3> transmission{.4, .6, .1};
    const double incident = 1000 * std::sqrt(.5) / std::numbers::pi;
    for (const bool mapped : {false, true}) {
      outshine::Material material;
      material.Scattering = outshine::ScatteringModel::DiffuseThinSheet;
      material.BaseColour = {{.1f, .2f, .03f, 1}};
      material.DiffuseTransmission = {{.4f, .6f, .1f}};
      material.DoubleSided = true;
      const auto geometry = SheetGeometry(material, mapped);
      const auto front = LightResponse(geometry, 45);
      const auto back = LightResponse(geometry, -45);
      const double green = mapped ? std::pow((128.0 / 255 + .055) / 1.055, 2.4) : 1.0;
      for (size_t channel = 0; channel < 3; ++channel) {
        const double texture = channel == 1 ? green : 1.0;
        CHECK_NEAR(front[channel],
                   reflection[channel] * incident * texture,
                   .002 * incident,
                   "linear radiance",
                   "front light matches independent diffuse reflection and the selected UV map");
        CHECK_NEAR(back[channel],
                   transmission[channel] * incident,
                   .002 * incident,
                   "linear radiance",
                   "back light matches independent diffuse transmission");
      }
      std::println("THIN_SHEET mapped={} front={:.6f},{:.6f},{:.6f} back={:.6f},{:.6f},{:.6f}",
                   mapped,
                   front[0],
                   front[1],
                   front[2],
                   back[0],
                   back[1],
                   back[2]);
    }
  }
  CheckMixture();
  SDL_Quit();
  return Report();
}
