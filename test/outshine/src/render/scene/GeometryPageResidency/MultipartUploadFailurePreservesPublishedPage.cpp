#include <cstddef>
#include <utility>
#include "GeometryPageResidency.h"
#include "SceneRenderer.h"
#include "GeometryPageView.h"
#include "Geometry.h"
#include "Material.h"
#include "StoredVertex.h"
#include "Check.h"
#include <SDL3/SDL.h>
#include <array>
#include <algorithm>
#include <cstdint>
#include <string>
#include <print>
#include <vector>

namespace {
void CheckPages() {
  using namespace outshine;
  using namespace outshine::Render;
  using namespace outshine::Test;
  constexpr uint32_t kPage = 7;
  constexpr uint64_t kSource = 9;
  constexpr uint32_t kMissingMaterial = 99;
  constexpr size_t kPartCount = 3;
  const auto plan = ExecutionPlan::Compile({.Outputs = {Resource::SceneLinear},
                                            .Content = {Stage::OpaqueGeometry},
                                            .Exposure = {},
                                            .Display = {},
                                            .Precision = {}});
  CHECK(plan, "native geometry execution plan compiles");
  if (!plan) { return; }
  SceneRenderer renderer;
  CHECK(renderer.Init({.WidthPx = 32, .HeightPx = 32}, *plan), "the native render target opens");
  Viewpoint camera;
  camera.Kind = CameraKind::Orthographic;
  camera.XMagM = camera.YMagM = 1;
  camera.ZNearM = .1;
  camera.ZFarM = 10;
  const auto lens = Lens::From(camera, 32, 32);
  CHECK(lens, "the page camera has a valid lens");
  if (!lens) { return; }
  renderer.SetCamera(camera, *lens);
  const auto picture = [&] {
    CHECK(renderer.RenderFrame(), "the resident pages render");
    renderer.WaitForGpu();
    std::vector<float> pixels;
    CHECK(renderer.ReadSceneLinear(pixels) == ReadState::Ready, "page pixels are available");
    return pixels;
  };
  Geometry materials;
  Material material;
  material.Unlit = material.DoubleSided = true;
  material.BaseColour = {{.125f, .5f, .25f, 1}};
  CHECK(materials.addSurface("native", material), "native material exists without a generator");
  const auto first = renderer.RegisterPieceMaterials(std::move(materials));
  CHECK(first, "native piece materials register");
  if (!first) { return; }
  const std::array<StoredVertex, 3> vertices{StoredVertex::Of({{0, 0, 2}}, {{0, 0}}, {{0, 0, -1}}),
                                             StoredVertex::Of({{1, 0, 2}}, {{1, 0}}, {{0, 0, -1}}),
                                             StoredVertex::Of({{0, 1, 2}}, {{0, 1}}, {{0, 0, -1}})};
  constexpr std::array<uint32_t, 3> indices{0, 1, 2};
  std::array<GeometryPagePartView, kPartCount> parts;
  for (auto &part : parts) {
    part = {.Tangents = {},
            .Verts = vertices,
            .Indices = indices,
            .Clusters = {},
            .Colours = {},
            .Uv1 = {},
            .Surface = PieceSurface::Registered(*first),
            .Textured = false};
  }
  GeometryPageView page{.Parts = parts,
                        .Digest = 1,
                        .AssetKey = {},
                        .FallbackHeights = false,
                        .SelectedView = {},
                        .RequestedDetail = LevelOfDetail::Fine,
                        .RequestedCell = {},
                        .OccupiedCells = 0};
  GeometryPageResidency residency;
  residency.BindRenderer(&renderer);
  const auto frame = TangentFrame::At({});
  residency.SetFrame(frame);
  std::string error;
  CHECK(residency.Upload(kPage, page, frame.OriginEcef(), error, kSource),
        "a page accepts three native mesh parts");
  const auto before = picture();
  CHECK(std::ranges::find(before, .5f) != before.end() && residency.ValidateSources(error),
        "the resident page paints its declared green material");
  parts.back().Surface = PieceSurface::Registered(*first, kMissingMaterial);
  page.Digest = 2;
  CHECK(!residency.Upload(kPage, page, frame.OriginEcef(), error, kSource),
        "the third upload refuses after two successful candidate uploads");
  CHECK(picture() == before && residency.ValidateSources(error),
        "refusal releases every candidate part and preserves the published page");
  parts.back().Surface = PieceSurface::Registered(*first);
  CHECK(residency.Upload(kPage, page, frame.OriginEcef(), error, kSource) && picture() == before &&
            residency.ValidateSources(error),
        "retry publishes the complete page with its original appearance");
  residency.Retire(kPage);
  const auto retired = picture();
  CHECK(!retired.empty() && std::ranges::all_of(retired, [](float value) { return value == 0; }),
        "retirement removes all page pixels");
}
}

int main() {
  using namespace outshine::Test;
  const bool video = SDL_Init(SDL_INIT_VIDEO);
  CHECK(video, "native video initializes");
  if (video) { CheckPages(); }
  SDL_Quit();
  return Report();
}
