#include "SceneLighting.h"
#include "Check.h"
#include "GeometryPageResidency.h"
#include "RuntimeScene.h"
#include "SceneRenderer.h"
#include <SDL3/SDL.h>
#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <print>
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
                       PrismHandle handle,
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
                       PrismHandle handle,
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
                             PrismHandle handle,
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
  std::shared_ptr<const PrismGrid> lastSource;
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
    lastSource = source;
    const auto image = Draw(scene, renderer);
    if (columns == 1) { baseline = image; }
    CHECK(image.Colour == baseline.Colour && image.Normal == baseline.Normal &&
              image.Depth == baseline.Depth,
          "facade filtering and shading are byte stable across 1/2/8/128 coplanar cells");
  }
  Geometry plainMaterials;
  CHECK(plainMaterials.addSurface("plain-reference", Material{}),
        "the reference disables only the facade pattern");
  const auto plainSlot = renderer.RegisterPieceMaterials(std::move(plainMaterials));
  CHECK(plainSlot, "the independent plain material is registered");
  if (!plainSlot) { return; }
  CHECK(renderer.ReplacePrismGrid(handle, lastSource, instances, *plainSlot),
        "the pattern comparison retains actual geometry, pose, camera and light");
  const auto plain = Draw(scene, renderer);
  CHECK(plain.Colour != baseline.Colour && plain.Depth == baseline.Depth,
        "the actual coating filter changes colour independently of lighting and geometry");
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
        "the visible lit facade has nontrivial colour contrast");
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

void CheckRoofDepth(const Viewpoint &eye, const Image &frame, const Image &reference) {
  CHECK(frame.Depth.size() == kPixels && frame.Normal.size() == kPixels * 4 &&
            frame.Colour.size() == kPixels * 4 && reference.Depth.size() == kPixels,
        "the Native GPU world returns the gable in the ordinary opaque pass");
  if (frame.Depth.size() != kPixels || frame.Normal.size() != kPixels * 4 ||
      frame.Colour.size() != kPixels * 4 || reference.Depth.size() != kPixels) {
    return;
  }
  size_t coverageErrors = 0;
  double depthError = 0;
  size_t shadedSlopes = 0;
  size_t retainedMeshPixels = 0;
  for (uint32_t row = 0; row < kSide; ++row) {
    for (uint32_t column = 0; column < kSide; ++column) {
      const size_t pixel = size_t{row} * kSide + column;
      const double x = (2 * (column + .5) / kSide - 1) * eye.XMagM;
      const double y = (1 - 2 * (row + .5) / kSide) * eye.YMagM;
      if (x > 2) {
        CHECK(frame.Depth[pixel] == reference.Depth[pixel], "the unsupported mesh keeps its depth");
        retainedMeshPixels += static_cast<size_t>(frame.Depth[pixel] > 0);
        continue;
      }
      const bool expected = std::abs(x) < 1 && y > -1 && y < 2.5;
      coverageErrors += static_cast<size_t>(expected != (frame.Depth[pixel] > 0));
      if (!expected) { continue; }
      const double z = std::min(1.0, 2 * (2.5 - y));
      const double expectedDepth = (kFarM - (eye.EyeM[2] - z)) / (kFarM - kNearM);
      depthError = std::max(depthError, std::abs(expectedDepth - frame.Depth[pixel]));
      if (y > 2 && std::abs(frame.Normal[pixel * 4 + 1] - 2 / std::sqrt(5.0)) < .002 &&
          std::abs(frame.Normal[pixel * 4 + 2] - 1 / std::sqrt(5.0)) < .002 &&
          frame.Colour[pixel * 4 + 1] > frame.Colour[pixel * 4]) {
        ++shadedSlopes;
      }
    }
  }
  std::println("CONVEX roof coverage_mismatch={} reverse_depth_error={} shaded_roof_pixels={}",
               coverageErrors,
               depthError,
               shadedSlopes);
  CHECK(coverageErrors == 0 && depthError < 2e-6 && shadedSlopes == 4 * 16 &&
            retainedMeshPixels > 0,
        "the coloured gable matches an independent silhouette, depth and sloped normal");
  CHECK(frame.Colour[kCentre * 4] > frame.Colour[kCentre * 4 + 1],
        "the original wall paint remains distinct from the tinted roof planes");
}

void CheckConvexRoof(Core::RuntimeScene &scene,
                     SceneRenderer &renderer,
                     Viewpoint eye,
                     const MaterialRegistration &material) {
  scene.Eye(eye);
  const auto baseline = Draw(scene, renderer);
  const ConvexPrism roof{.SceneFromLocal = {},
                         .Bounds = {.Min = {{-1, -1, -1}}, .Max = {{1, 2.5f, 1}}},
                         .Planes = {{{-1, 0, 0, 1},
                                     {1, 0, 0, 1},
                                     {0, 0, -1, 1},
                                     {0, 0, 1, 1},
                                     {0, -1, 0, 1},
                                     {0, 1, .5f, 2.5f},
                                     {0, 1, -.5f, 2.5f}}},
                         .SecondaryFaces = (1u << 5u) | (1u << 6u),
                         .Secondary = {{.125f, 1, 1, .6f}}};
  GeometryPageResidency pages;
  pages.BindRenderer(&renderer);
  constexpr uint32_t tile = 17;
  constexpr uint64_t revision = 3;
  constexpr std::array<uint32_t, 2> ordinals{0, 1};
  const auto coverage = SourceCoverage::FromSources("test-tile", 2, ordinals);
  CHECK(coverage, "the tile has an immutable source address");
  if (!coverage) { return; }
  std::string error;
  const auto placement = std::make_shared<const Mat4>();
  constexpr std::array<uint32_t, 3> triangles{0, 1, 2};
  const std::array points{StoredVertex::Of({{-1, -1, 0}}, {}, {{0, 0, 1}}),
                          StoredVertex::Of({{1, -1, 0}}, {}, {{0, 0, 1}}),
                          StoredVertex::Of({{0, 1, 0}}, {}, {{0, 0, 1}})};
  const std::array remainder{StoredVertex::Of({{2.6f, -2, 0}}, {}, {{0, 0, 1}}),
                             StoredVertex::Of({{3.4f, -2, 0}}, {}, {{0, 0, 1}}),
                             StoredVertex::Of({{3, -1, 0}}, {}, {{0, 0, 1}})};
  const auto surface = PieceSurface::Registered(material);
  const GeometryPagePartView original{.Tangents = {},
                                      .Verts = points,
                                      .Indices = triangles,
                                      .Clusters = {},
                                      .Colours = {},
                                      .Uv1 = {},
                                      .Surface = surface};
  auto remaining = original;
  remaining.Verts = remainder;
  const std::array mesh{original, remaining};
  const std::array<std::string_view, 1> parent{"parent"};
  const std::array<std::string_view, 1> child{"roof"};
  auto page = GeometryPageView{.Parts = mesh,
                               .Digest = 1,
                               .AssetKey = parent.front(),
                               .SelectedView = std::nullopt,
                               .RequestedDetail = LevelOfDetail::Fine,
                               .RequestedCell = 1,
                               .OccupiedCells = 1,
                               .Coverage = *coverage,
                               .Placement = placement};
  CHECK(pages.StageProduct(tile, page, {}, revision) &&
            pages.ActivateProducts(tile, revision, *coverage, parent, parent, error),
        "the original mesh covers the complete tile");
  const auto parentFrame = Draw(scene, renderer);
  CHECK(parentFrame.Depth != baseline.Depth, "the original mesh is visible");
  CHECK(!pages.ActivateProducts(tile, revision, *coverage, child, parent, error),
        "a missing analytic batch cannot displace the resident mesh");
  page.Parts = std::span(mesh).last<1>();
  page.Digest = 2;
  page.AssetKey = child.front();
  page.Prism = GeometryPagePrismView{.Shapes = std::span(&roof, 1), .Material = {}};
  CHECK(!pages.StageProduct(tile, page, {}, revision),
        "a late invalid prism refuses the entire product after uploading the mesh");
  const auto refused = Draw(scene, renderer);
  CHECK(refused.Depth == parentFrame.Depth && refused.Shadow == parentFrame.Shadow,
        "the failed mixed upload leaves the original tile published");
  page.Prism->Material = material;
  CHECK(pages.StageProduct(tile, page, {}, revision), "the replacement batch uploads while hidden");
  const auto hidden = Draw(scene, renderer);
  CHECK(hidden.Depth == parentFrame.Depth && hidden.Shadow == parentFrame.Shadow,
        "staging leaves both passes of the complete resident mesh intact");
  CHECK(pages.ActivateProducts(tile, revision, *coverage, child, parent, error),
        "the complete replacement takes over the main pass only");
  const auto mainOnly = Draw(scene, renderer);
  CHECK(mainOnly.Depth != parentFrame.Depth && mainOnly.Shadow == parentFrame.Shadow,
        "the same cut retains the original sun caster while replacing the visible geometry");
  CHECK(pages.ActivateProducts(tile, revision, *coverage, child, child, error),
        "the analytic batch can also replace the original caster");
  const auto frame = Draw(scene, renderer);
  CheckRoofDepth(eye, frame, parentFrame);
  CHECK(frame.Shadow != baseline.Shadow &&
            std::ranges::any_of(frame.Shadow, [](float depth) { return depth > 0; }),
        "the gable casts a shadow using its actual sloped planes");
  eye.EyeM[0] += .5;
  scene.Eye(eye);
  const auto moved = Draw(scene, renderer);
  CHECK(moved.Velocity.size() > kCentre * 2 &&
            std::abs(moved.Velocity[kCentre * 2]) > kMovingMotion,
        "camera movement reprojects the stationary analytic surface");
  pages.Retire(tile);
  const auto released = Draw(scene, renderer);
  CHECK(released.Depth == baseline.Depth && released.Shadow == baseline.Shadow,
        "releasing the gable removes its opaque depth and its cached shadow");
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
  auto offscreen = instances;
  offscreen[0].Current[12] += 100;
  CHECK(renderer.SetPrismGridInstances(*handle, offscreen),
        "an existing prism can move outside the camera");
  const auto outside = Draw(*scene, renderer);
  CHECK(outside.Depth.size() > kCentre && outside.Depth[kCentre] == 0,
        "the moved prism stops covering its previous pixels");
  CHECK(renderer.SetPrismGridInstances(*handle, instances),
        "a resident prism can re-enter the camera");
  const auto returned = Draw(*scene, renderer);
  CHECK(returned.Depth == first.Depth && returned.Shadow == first.Shadow,
        "camera culling follows current placements and preserves independent caster coverage");
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
  const PrismSpan nearCell{.Lower = -1, .Upper = 1};
  const PrismSpan farCell{.Lower = -1, .Upper = 2};
  const PrismSpan excluded{.Lower = -1, .Upper = 9};
  const auto dense = std::make_shared<const PrismGrid>(
      PrismGrid{.Samples = std::make_shared<const std::vector<PrismSpan>>(
                    std::initializer_list<PrismSpan>{nearCell, farCell}),
                .Patches = {{.Columns = 1, .Rows = 2, .PitchX = 2, .PitchZ = 1}}});
  const auto strided = std::make_shared<const PrismGrid>(PrismGrid{
      .Samples = std::make_shared<const std::vector<PrismSpan>>(
          std::initializer_list<PrismSpan>{excluded, nearCell, excluded, farCell}),
      .Patches = {
          {.FirstSample = 1, .Columns = 1, .Rows = 2, .PitchX = 2, .PitchZ = 1, .RowStride = 2}}});
  CHECK(renderer.ReplacePrismGrid(*handle, dense, instances, *slot),
        "both rows are available as a dense control");
  const auto beforeStride = Draw(*scene, renderer);
  CHECK(renderer.ReplacePrismGrid(*handle, strided, instances, *slot),
        "the same rows use a cropped view of a wider immutable source");
  const auto afterStride = Draw(*scene, renderer);
  CHECK(beforeStride.Depth == afterStride.Depth && beforeStride.Normal == afterStride.Normal &&
            beforeStride.Shadow == afterStride.Shadow,
        "the native trace skips the excluded columns in colour and shadow");
  CheckMappedNormal(*scene, renderer, *handle, instances);
  CheckMipFiltering(*scene, renderer, *handle, instances);
  CheckFacadePartitioning(*scene, renderer, *handle, instances);
  renderer.ReleasePrism(*handle);
  const auto released = Draw(*scene, renderer);
  CHECK(renderer.Spent(Stage::OpaqueGeometry).DeviceBytes == nativeBytes,
        "release removes prism payload from the geometry memory diagnostic");
  CHECK(released.Depth.size() > kCentre && released.Depth[kCentre] == 0 &&
            std::ranges::none_of(released.Shadow, [](float depth) { return depth > 0; }),
        "release removes colour depth and cached caster depth without ghost geometry");
  light.Light.Intensity = kKeyLux;
  CHECK(renderer.SetGeometryLights(std::span(&light, 1), error), "the gable has a visible key");
  CheckConvexRoof(*scene, renderer, *eye, *slot);
}
}

int main() {
  CHECK(SDL_Init(SDL_INIT_VIDEO), "SDL video initializes");
  CheckScene();
  SDL_Quit();
  return Report();
}
