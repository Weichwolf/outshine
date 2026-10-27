#include "SceneLighting.h"
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
#include <limits>
#include <string>
#include <vector>

namespace {
using namespace outshine;
using namespace outshine::Render;
using namespace outshine::Test;
constexpr uint32_t kSide = 64;
constexpr size_t kPixels = size_t{kSide} * kSide;
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
  return std::make_shared<const PrismGrid>(PrismGrid{
      .Samples = std::make_shared<const std::vector<PrismSpan>>(
          std::vector<PrismSpan>{{.Lower = -1, .Upper = upper}}),
      .Patches = {{.Columns = 1, .Rows = 1, .PitchX = 2, .PitchZ = 2, .FillFraction = .61f}}});
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

void CheckTraceBoundaryQuads(const Image &image) {
  if (image.Depth.size() != kPixels) { return; }
  constexpr uint32_t first = kSide / 2 - kSide / 8;
  constexpr uint32_t end = kSide / 2 + kSide / 8;
  size_t mixed = 0;
  for (uint32_t y = first + 2; y + 1 < end - 1; y += 2) {
    for (uint32_t x = first + 2; x + 1 < end - 1; x += 2) {
      if (x == y || x + y == kSide - 2) { continue; }
      size_t covered = 0;
      for (const auto dy : {0u, 1u}) {
        for (const auto dx : {0u, 1u}) {
          if (image.Depth[size_t{y + dy} * kSide + x + dx] > 0) { ++covered; }
        }
      }
      if (covered > 0 && covered < 4) { ++mixed; }
    }
  }
  CHECK(mixed > 0,
        "actual trace hit/miss quads occur strictly inside the proxy and away from both diagonals");
}

Geometry FilteredMaterial(bool reference) {
  Geometry geometry;
  constexpr uint32_t width = 128;
  std::array<uint8_t, size_t{width} * 4> colour{};
  std::array<uint8_t, size_t{width} * 4> normal{};
  for (size_t i = 0; i < width; ++i) {
    const auto level = static_cast<uint8_t>((i % 2) * 255);
    const auto tilt = static_cast<uint8_t>(i % 2 != 0 ? 220 : 36);
    std::copy_n(
        std::array<uint8_t, 4>{level, level, level, 255}.begin(), 4, colour.begin() + i * 4);
    std::copy_n(std::array<uint8_t, 4>{tilt, 220, 128, 255}.begin(), 4, normal.begin() + i * 4);
  }
  constexpr std::array<uint8_t, 4> grey{188, 188, 188, 255};
  const auto image =
      reference ? geometry.addImage(1, 1, grey) : geometry.addImage(width, 1, colour);
  const auto mapped = geometry.addImage(width, 1, normal);
  CHECK(image && mapped, "colour and normal filtering have independent native image inputs");
  if (!image || !mapped) { return {}; }
  Material material;
  material.BaseColour = {{1, 1, 1, 1}};
  material.Emission = {{1, 0, 0}};
  material.BaseColourMap.Image = material.EmissiveMap.Image = *image;
  material.NormalMap.Image = *mapped;
  material.BaseColourMap.Set = UvSet::Uv1;
  for (auto *map : {&material.BaseColourMap, &material.EmissiveMap, &material.NormalMap}) {
    map->Sampler.Minify = map->Sampler.Magnify = Filter::Linear;
    map->Sampler.Mip = MipFilter::Linear;
    map->Sampler.WrapU = map->Sampler.WrapV = Wrap::Repeat;
    map->Uv.OffsetUv = {{.137, .31}};
    map->Uv.ScaleUv = {{2, -1}};
    map->Uv.RotationRad = .37;
  }
  CHECK(geometry.addSurface("filtered-prism", material).has_value(),
        "the material samples transformed colour, emissive and normal maps with mips");
  return geometry;
}

void CheckMipFiltering(Core::RuntimeScene &scene,
                       SceneRenderer &renderer,
                       PrismGridHandle handle,
                       std::span<const PrismInstance> instances) {
  const auto draw = [&](bool reference) {
    const auto slot = renderer.RegisterPieceMaterials(FilteredMaterial(reference));
    CHECK(slot, "the filtered or independently averaged material is registered");
    if (!slot) { return Image{}; }
    CHECK(renderer.ReplacePrismGrid(handle, Source(1), instances, *slot),
          "only the material changes for the same actual partial-fill prism");
    return Draw(scene, renderer);
  };
  const auto sampled = draw(false);
  const auto average = draw(true);
  CHECK(sampled.Colour.size() == kPixels * 4 && sampled.Normal.size() == kPixels * 4 &&
            sampled.Depth.size() == kPixels && average.Colour.size() == sampled.Colour.size(),
        "actual filtered product outputs are complete");
  if (sampled.Colour.size() != kPixels * 4 || sampled.Normal.size() != kPixels * 4 ||
      sampled.Depth.size() != kPixels || average.Colour.size() != sampled.Colour.size()) {
    return;
  }
  size_t visible = 0;
  size_t errors = 0;
  size_t mapped = 0;
  for (size_t i = 0; i < sampled.Depth.size(); ++i) {
    if (sampled.Depth[i] <= 0) { continue; }
    ++visible;
    if (sampled.Normal[i * 4 + 1] > .99f) { ++mapped; }
    for (size_t c = 0; c < 3; ++c) {
      if (!std::isfinite(sampled.Colour[i * 4 + c]) ||
          std::abs(sampled.Colour[i * 4 + c] - average.Colour[i * 4 + c]) > 2e-5f) {
        ++errors;
      }
    }
  }
  CHECK(visible > 0 && mapped == visible,
        "every actual hit uses the filtered symmetric normal map, including trace boundaries");
  CHECK(errors == 0,
        "minified black/white colour and emission equal their independent linear average");
  CHECK(sampled.Depth == average.Depth, "filtering never changes the traced geometry depth");
}

void CheckFacadePartitioning(Core::RuntimeScene &scene,
                             SceneRenderer &renderer,
                             PrismGridHandle handle,
                             std::span<const PrismInstance> instances) {
  auto eye = Viewpoint::LookAt({.EyeM = {{0, 3, 10}}, .AimM = {{0, 3, 0}}}, 0.0);
  CHECK(eye, "the facade control observes multiple complete storeys");
  if (!eye) { return; }
  eye->Kind = CameraKind::Orthographic;
  eye->XMagM = eye->YMagM = 4;
  eye->ZNearM = kNearM;
  eye->ZFarM = kFarM;
  scene.Eye(*eye);
  Geometry materials;
  Material material;
  material.Pattern = SurfacePattern::FacadeMetres;
  CHECK(materials.addSurface("continuous-facade", material),
        "the shared facade filter is selected");
  const auto slot = renderer.RegisterPieceMaterials(std::move(materials));
  CHECK(slot, "the facade material reaches the original native pipeline");
  if (!slot) { return; }
  SceneLight light;
  light.Light.Intensity = kKeyLux;
  light.Light.Direction = {{0, 0, -1}};
  std::string error;
  CHECK(renderer.SetGeometryLights(std::span(&light, 1), error),
        "the facade has a fixed frontal key");
  Image baseline;
  for (const uint32_t columns : {1u, 2u, 8u, 128u}) {
    const auto source = std::make_shared<const PrismGrid>(
        PrismGrid{.Samples = std::make_shared<const std::vector<PrismSpan>>(
                      columns, PrismSpan{.Lower = -1, .Upper = 8}),
                  .Patches = {{.Columns = columns,
                               .Rows = 1,
                               .PitchX = 2.f / static_cast<float>(columns),
                               .PitchZ = 2}}});
    CHECK(renderer.ReplacePrismGrid(handle, source, instances, *slot),
          "only the cells partitioning one continuous facade change");
    const auto image = Draw(scene, renderer);
    if (columns == 1) { baseline = image; }
    CHECK(image.Colour == baseline.Colour && image.Normal == baseline.Normal &&
              image.Depth == baseline.Depth,
          "facade filtering and shading are byte stable across 1/2/8/128 coplanar cells");
  }
  CHECK(baseline.Colour.size() == kPixels * 4 && baseline.Depth.size() == kPixels,
        "the actual facade control has complete outputs");
  if (baseline.Colour.size() != kPixels * 4 || baseline.Depth.size() != kPixels) { return; }
  float minimum = std::numeric_limits<float>::infinity();
  float maximum = 0;
  size_t visible = 0;
  for (size_t i = 0; i < kPixels; ++i) {
    if (baseline.Depth[i] <= 0) { continue; }
    ++visible;
    minimum = std::min(minimum, baseline.Colour[i * 4]);
    maximum = std::max(maximum, baseline.Colour[i * 4]);
  }
  CHECK(visible > 0 && maximum - minimum > 1e-4f,
        "the visible facade signal has actual filtered contrast rather than a flat substitute");
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

void CheckNativeMaterialRemap(Core::RuntimeScene &scene,
                              SceneRenderer &renderer,
                              const SceneLight &light) {
  std::array<MaterialBinding, 2> materials;
  materials[1].Row.Unlit = true;
  std::string error;
  CHECK(renderer.SetGeometryMaterials(materials, error) && renderer.RestorePieceMaterials(error),
        error.empty() ? "native material slots move while geometry and prisms remain resident"
                      : error.c_str());
  CHECK(renderer.SetGeometryLights(std::span(&light, 1), error),
        "the same frontal light reaches the rebound scene");
  const auto rebound = Draw(scene, renderer);
  CHECK(rebound.Depth.size() > kCentre && rebound.Depth[kCentre] > 0 &&
            rebound.Colour.size() > kCentre * 4 + 2 &&
            rebound.Colour[kCentre * 4] > rebound.Colour[kCentre * 4 + 1],
        "a retained prism still draws its own red PBR material after physical slots move");
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
  const auto nativeBytes = renderer.Spent(Stage::OpaqueGeometry).DeviceBytes;
  constexpr uint64_t prismBytes = sizeof(PrismSpan) + sizeof(GpuPrismPatch);
  const auto handle = renderer.PlacePrismGrid(Source(1), instances, *slot);
  CHECK(handle.has_value(), "one prism becomes resident through the scene handle API");
  if (!handle) { return; }
  SceneLight light;
  light.Light.Intensity = kKeyLux;
  light.Light.Direction = {{0, 0, -1}};
  CHECK(renderer.SetGeometryLights(std::span(&light, 1), error),
        "a frontal key illuminates the independently chosen front face");
  const auto first = Draw(*scene, renderer);
  CheckTraceBoundaryQuads(first);
  CHECK(renderer.Spent(Stage::OpaqueGeometry).DeviceBytes == nativeBytes + prismBytes,
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
  CheckNativeMaterialRemap(*scene, renderer, light);
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
  CheckMipFiltering(*scene, renderer, *handle, instances);
  CheckFacadePartitioning(*scene, renderer, *handle, instances);
  renderer.ReleasePrismGrid(*handle);
  const auto released = Draw(*scene, renderer);
  CHECK(renderer.Spent(Stage::OpaqueGeometry).DeviceBytes == nativeBytes,
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
