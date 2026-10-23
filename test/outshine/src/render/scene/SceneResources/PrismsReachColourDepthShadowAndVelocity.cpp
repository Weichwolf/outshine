#include "Check.h"
#include "RuntimeScene.h"
#include "SceneRenderer.h"
#include <SDL3/SDL.h>
#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <span>
#include <utility>
#include <memory>
#include <string>
#include <vector>

namespace {
using namespace outshine;
using namespace outshine::Render;
using namespace outshine::Test;
constexpr uint32_t kSide = 64;
constexpr float kKeyLux = 10000;
constexpr double kSunElevationDeg = 35;
constexpr double kNearM = .1, kFarM = 100;
constexpr Vec4f kBaseColour{{.7f, .2f, .1f, 1}};
constexpr float kStillMotion = .001f, kMovingMotion = .01f;
constexpr size_t kCentre = size_t{kSide / 2} * kSide + kSide / 2;

struct Image {
  std::vector<float> Colour, Depth, Velocity, Shadow, Normal;
};

Image Draw(Core::RuntimeScene &scene, SceneRenderer &renderer) {
  std::string error;
  CHECK(scene.Draw(error), error.empty() ? "prism scene submits" : error.c_str());
  renderer.WaitForGpu();
  Image image;
  CHECK(renderer.ReadSceneLinear(image.Colour) == ReadState::Ready &&
            renderer.ReadDepth(image.Depth) == ReadState::Ready &&
            renderer.ReadSceneVelocity(image.Velocity) == ReadState::Ready &&
            renderer.ReadShadowAtlas(image.Shadow) == ReadState::Ready &&
            renderer.ReadShadingNormal(image.Normal) == ReadState::Ready,
        "the shared scene produces colour, depth, velocity and shadow readbacks");
  return image;
}

std::shared_ptr<const PrismGrid> Source(float upper) {
  return std::make_shared<const PrismGrid>(
      PrismGrid{.Samples = std::make_shared<const std::vector<PrismSpan>>(
                    std::vector<PrismSpan>{{.Lower = -1, .Upper = upper}}),
                .Patches = {{.Columns = 1, .Rows = 1, .PitchX = 2, .PitchZ = 2}}});
}

void CheckMappedNormal(Core::RuntimeScene &scene,
                       SceneRenderer &renderer,
                       PrismGridHandle handle,
                       std::span<const PrismInstance> instances) {
  Geometry materials;
  constexpr std::array<uint8_t, 4> texel{128, 255, 128, 255};
  constexpr float mappedMinimum = .99f;
  const auto image = materials.addImage(1, 1, texel);
  CHECK(image.has_value(), "an explicit normal map is available");
  if (!image) { return; }
  Material material;
  material.NormalMap.Image = *image;
  CHECK(materials.addSurface("mapped-prism", material).has_value(),
        "the replacement material carries the actual normal map");
  const auto slot = renderer.RegisterPieceMaterials(std::move(materials));
  CHECK(slot.has_value(), "the mapped material is registered");
  if (!slot) { return; }
  CHECK(renderer.ReplacePrismGrid(handle, Source(1), instances, *slot).has_value(),
        "the source and material replace atomically through the retained handle");
  const auto mapped = Draw(scene, renderer);
  CHECK(mapped.Normal.size() > kCentre * 4 + 2 && mapped.Normal[kCentre * 4 + 1] > mappedMinimum,
        "an actual tangent-space normal map still changes the geometric face normal");
}

Geometry NativeMaterialOwner() {
  Geometry geometry;
  const auto part = geometry.addPart("native-material-owner", MaterialInstance{});
  CHECK(part.has_value(), "a native mesh owns a material before piece registration");
  if (!part) { return {}; }
  constexpr float distantM = 100000;
  constexpr std::array<float, 9> positions{distantM, 0, 0, distantM + 1, 0, 0, distantM, 1, 0};
  CHECK(geometry.setPositions(*part, positions) &&
            geometry.setTriangles(*part, std::array<uint32_t, 3>{0, 1, 2}),
        "the independent native material owner stays outside the camera and shadow bounds");
  return geometry;
}

void CheckScene() {
  SceneRenderer renderer;
  const auto native = NativeMaterialOwner();
  Core::Declaration declaration;
  declaration.InitialGeometry = &native;
  declaration.SurfaceWidthPx = declaration.SurfaceHeightPx = kSide;
  declaration.Outputs = {
      "surface", "sceneLinear", "sceneDepth", "sceneVelocity", "shadowAtlas", "sceneShadingNormal"};
  declaration.KeyLux = kKeyLux;
  declaration.KeyElevationDeg = kSunElevationDeg;
  declaration.KeyBearingDeg = 0;
  std::unique_ptr<Core::RuntimeScene> scene;
  std::string error;
  CHECK(Core::RuntimeScene::Open(renderer, declaration, nullptr, scene, error),
        error.empty() ? "the native prism scene opens" : error.c_str());
  if (!scene) { return; }
  auto eye = Viewpoint::LookAt({.EyeM = {{0, 0, 10}}, .AimM = {{0, 0, 0}}}, 0.0);
  CHECK(eye.has_value(), "an independent frontal camera exists");
  if (!eye) { return; }
  eye->Kind = CameraKind::Orthographic;
  eye->XMagM = eye->YMagM = 4;
  eye->ZNearM = kNearM;
  eye->ZFarM = kFarM;
  scene->Eye(*eye);
  renderer.SetShadowFrame({{0, 0, 1}}, {{0, 1, 0}}, 512, true, 0);
  Geometry materials;
  Material material;
  material.BaseColour = kBaseColour;
  material.Roughness = 1;
  CHECK(materials.addSurface("prism", material).has_value(),
        "the prism has a coloured PBR material");
  const auto slot = renderer.RegisterPieceMaterials(std::move(materials));
  CHECK(slot.has_value(), "the material reaches the shared scene registry");
  if (!slot) { return; }
  std::array<PrismInstance, 1> instances{};
  instances[0].Current[12] = instances[0].Current[14] = -1;
  instances[0].Previous = instances[0].Current;
  (void)Draw(*scene, renderer);
  const auto nativeBytes = renderer.Spent(Stage::Geometry).DeviceBytes;
  constexpr uint64_t prismBytes = sizeof(PrismSpan) + sizeof(GpuPrismPatch);
  const auto handle = renderer.PlacePrismGrid(Source(1), instances, *slot);
  CHECK(handle.has_value(), "one prism becomes resident through the scene handle API");
  if (!handle) { return; }
  SubjectLight light;
  light.Light.Intensity = kKeyLux;
  light.Light.Direction = {{0, 0, -1}};
  CHECK(renderer.SetGeometryLights(std::span(&light, 1), error),
        "a frontal key illuminates the independently chosen front face");
  const auto first = Draw(*scene, renderer);
  CHECK(renderer.Spent(Stage::Geometry).DeviceBytes == nativeBytes + prismBytes,
        "geometry diagnostics include the live prism sample and record buffer payload");
  if (first.Depth.size() <= kCentre || first.Colour.size() <= kCentre * 4 + 2 ||
      first.Velocity.size() <= kCentre * 2 + 1) {
    return;
  }
  CHECK(first.Depth[kCentre] > 0 && first.Colour[kCentre * 4] > first.Colour[kCentre * 4 + 1] &&
            first.Colour[kCentre * 4 + 1] > 0,
        "the traced surface writes depth and the registered red PBR material");
  CHECK(first.Normal.size() > kCentre * 4 + 2 &&
            std::abs(first.Normal[kCentre * 4]) < kStillMotion &&
            std::abs(first.Normal[kCentre * 4 + 1]) < kStillMotion &&
            first.Normal[kCentre * 4 + 2] > 1 - kStillMotion,
        "an absent normal map preserves the geometric front-face normal");
  CHECK(std::abs(first.Velocity[kCentre * 2]) < kStillMotion &&
            std::abs(first.Velocity[kCentre * 2 + 1]) < kStillMotion,
        "a stationary prism and camera have zero motion");
  CHECK(std::ranges::any_of(first.Shadow, [](float depth) { return depth > 0; }),
        "the native prism enters the shared sun caster pass");
  instances[0].Previous = instances[0].Current;
  instances[0].Current[12] += .5f;
  CHECK(renderer.SetPrismGridInstances(*handle, instances).has_value(),
        "a changed pose is accepted without replacing samples");
  const auto moved = Draw(*scene, renderer);
  CHECK(moved.Velocity.size() > kCentre * 2 + 1 &&
            std::abs(moved.Velocity[kCentre * 2]) > kMovingMotion,
        "the actual traced hit carries object motion into the shared velocity output");
  CHECK(moved.Shadow != first.Shadow, "a pose change invalidates the cached caster depth");
  light.Light.Intensity = 0;
  CHECK(renderer.SetGeometryLights(std::span(&light, 1), error),
        "the existing light setter applies");
  const auto dark = Draw(*scene, renderer);
  CHECK(dark.Colour.size() > kCentre * 4 && dark.Colour[kCentre * 4] < moved.Colour[kCentre * 4],
        "the prism responds to a changed light through the common PBR implementation");
  CheckMappedNormal(*scene, renderer, *handle, instances);
  renderer.ReleasePrismGrid(*handle);
  const auto released = Draw(*scene, renderer);
  CHECK(renderer.Spent(Stage::Geometry).DeviceBytes == nativeBytes,
        "release removes prism payload from the geometry memory diagnostic");
  CHECK(released.Depth.size() > kCentre && released.Depth[kCentre] == 0 &&
            std::ranges::none_of(released.Shadow, [](float depth) { return depth > 0; }),
        "release removes colour depth and cached caster depth without ghost geometry");
}
}

int main() {
  CHECK(SDL_Init(SDL_INIT_VIDEO), "SDL video initializes");
  CheckScene();
  SDL_Quit();
  return Report();
}
