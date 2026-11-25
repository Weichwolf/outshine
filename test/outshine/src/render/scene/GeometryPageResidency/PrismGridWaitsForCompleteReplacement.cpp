#include "GeometryPageResidency.h"
#include "PrismGrid.h"
#include "PrismInstance.h"
#include "SceneRenderer.h"
#include "Check.h"
#include "test/harness/render/NativeGpuCheck.h"
#include <SDL3/SDL.h>
#include <algorithm>
#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace {
using namespace outshine;
using namespace outshine::Render;
using namespace outshine::Test;
constexpr uint32_t kTile = 7;
constexpr uint64_t kSource = 9;
constexpr size_t kSide = 32;

PrismInstance At(float x, float z) {
  PrismInstance row;
  row.Current.SetTranslation(Vec3f{{x, 0, z}});
  row.Previous = row.Current;
  return row;
}

std::vector<float> DrawDepth(SceneRenderer &renderer) {
  CHECK(renderer.RenderFrame(), "the scene submits its selected building groups");
  renderer.WaitForGpu();
  std::vector<float> depth;
  CHECK(renderer.ReadDepth(depth) == ReadState::Ready && depth.size() == kSide * kSide,
        "the main depth shows the visible source coverage");
  if (depth.size() != kSide * kSide) { outshine::Test::Gpu::Failed(); }
  return depth;
}

void CheckSelection(SceneRenderer &renderer, MaterialRegistration material) {
  GeometryPageResidency pages;
  pages.BindRenderer(&renderer);
  const auto frame = TangentFrame::At({});
  pages.SetFrame(frame);
  const auto grid = std::make_shared<const PrismGrid>(PrismGrid{
      .Samples = std::make_shared<const std::vector<PrismSpan>>(
          std::vector<PrismSpan>{{.Lower = -.55f, .Upper = .55f}}),
      .Patches = {{.FirstSample = 0, .Columns = 1, .Rows = 1, .PitchX = .6f, .PitchZ = .5f}}});
  const auto left = SourceCoverage::FromSources("blocks", 2, std::array<uint32_t, 1>{0});
  const auto right = SourceCoverage::FromSources("blocks", 2, std::array<uint32_t, 1>{1});
  const auto both = SourceCoverage::FromSources("blocks", 2, std::array<uint32_t, 2>{0, 1});
  CHECK(left && right && both, "the two building groups partition the parent");
  if (!left || !right || !both) { return; }
  const auto stage = [&](std::string_view key,
                         const SourceCoverage &coverage,
                         std::span<const PrismInstance> rows,
                         GeometryPasses passes) {
    GeometryPageView product;
    product.AssetKey = key;
    product.Coverage = coverage;
    product.Grid = GeometryPageGridView{
        .Grid = grid, .SceneInstances = rows, .Material = material, .Passes = passes};
    return pages.StageProduct(kTile, product, frame.OriginEcef(), kSource).has_value();
  };
  constexpr std::array<std::string_view, 1> parent{"parent"};
  constexpr std::array<std::string_view, 2> children{"left", "right"};
  const std::array original{At(-.8f, -2.5f), At(.2f, -2.5f)};
  std::string error;
  CHECK(stage("parent", *both, original, GeometryPasses::MainAndShadow) &&
            pages.ActivateProducts(kTile, kSource, *both, parent, parent, error),
        "the complete coarse source draws in both passes");
  const auto before = DrawDepth(renderer);
  std::vector<float> oldShadow;
  CHECK(renderer.ReadShadowAtlas(oldShadow) == ReadState::Ready &&
            std::ranges::any_of(oldShadow, [](float depth) { return depth > 0; }),
        "the original shadow atlas is available");
  const auto fine = At(-.8f, -1.6f);
  CHECK(stage("left", *left, {&fine, 1}, GeometryPasses::Main) && DrawDepth(renderer) == before,
        "a staged front building is hidden while its sibling is missing");
  CHECK(!pages.ActivateProducts(kTile, kSource, *both, children, parent, error) &&
            DrawDepth(renderer) == before,
        "partial replacement keeps the coarse building group visible");
  CHECK(stage("right", *right, std::span(original).last(1), GeometryPasses::Main) &&
            pages.ActivateProducts(kTile, kSource, *both, children, parent, error),
        "both children replace the parent only in the main pass");
  const auto displayed = DrawDepth(renderer);
  constexpr size_t leftPixel = (kSide / 2) * kSide + kSide / 4;
  constexpr size_t rightPixel = (kSide / 2) * kSide + 3 * (kSide / 4);
  CHECK(before[leftPixel] > 0 && before[rightPixel] > 0 &&
            displayed[leftPixel] != before[leftPixel] &&
            displayed[rightPixel] == before[rightPixel],
        "the nearer child appears while its sibling keeps the same coverage");
  std::vector<float> shadow;
  CHECK(renderer.ReadShadowAtlas(shadow) == ReadState::Ready && shadow == oldShadow,
        "the main replacement preserves the parent shadow coverage");
  CHECK(pages.ActivateProducts(kTile, kSource, *both, parent, parent, error) &&
            DrawDepth(renderer) == before,
        "returning to the parent hides both finer groups");
}

void Exercise() {
  SceneRenderer renderer;
  const auto uploads = renderer.OfflineUploads(std::chrono::seconds(5));
  const auto plan = ExecutionPlan::Compile(
      {.Outputs = {Resource::SceneLinear, Resource::SceneDepth, Resource::ShadowAtlas},
       .Content = {Stage::OpaqueGeometry, Stage::LightVisibility},
       .Exposure = {},
       .Display = {},
       .Precision = {}});
  CHECK(plan && renderer.Init({.WidthPx = kSide, .HeightPx = kSide}, *plan),
        "the native world target opens");
  if (!plan || renderer.PictureW() != kSide) { return; }
  Viewpoint camera;
  camera.Kind = CameraKind::Orthographic;
  camera.XMagM = camera.YMagM = 1;
  camera.ZNearM = .1;
  camera.ZFarM = 10;
  const auto lens = Lens::From(camera, kSide, kSide);
  CHECK(lens, "the local world camera has a valid lens");
  if (!lens) { return; }
  renderer.SetCamera(camera, *lens);
  renderer.SetShadowFrame({{0, 0, 1}}, {{0, 1, 0}}, 16384, true, 0);
  Geometry geometry;
  CHECK(geometry.addSurface("building group", Material{}), "the grid uses an opaque material");
  auto material = renderer.RegisterPieceMaterials(std::move(geometry));
  CHECK(material, "the grid material registers on the native renderer");
  if (material) { CheckSelection(renderer, *material); }
}
}

int main() {
  CHECK(SDL_Init(SDL_INIT_VIDEO), "native world pages require video");
  Exercise();
  SDL_Quit();
  return Report();
}
