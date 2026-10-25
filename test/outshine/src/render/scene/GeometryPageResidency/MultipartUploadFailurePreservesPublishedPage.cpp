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
#include <cstdint>
#include <string>
#include <print>

namespace {
void CheckPages() {
  using namespace outshine;
  using namespace outshine::Render;
  using namespace outshine::Test;
  constexpr uint32_t kPage = 7;
  constexpr uint64_t kSource = 9;
  constexpr uint32_t kMissingMaterial = 99;
  constexpr size_t kPartCount = 3;
  const auto plan = ExecutionPlan::Compile({.Outputs = {Resource::SceneHdr},
                                            .Content = {Stage::Geometry},
                                            .Exposure = {},
                                            .Display = {},
                                            .Precision = {}});
  CHECK(plan, "native geometry execution plan compiles");
  if (!plan) { return; }
  SceneRenderer renderer;
  CHECK(renderer.Init({.WidthPx = 32, .HeightPx = 32}, *plan), "the native render target opens");
  Geometry materials;
  CHECK(materials.addSurface("native", Material{}), "native material exists without a generator");
  const auto first = renderer.RegisterPieceMaterials(std::move(materials));
  CHECK(first, "native piece materials register");
  if (!first) { return; }
  const std::array<StoredVertex, 3> vertices{StoredVertex::Of({{0, 0, 0}}, {{0, 0}}, {{0, 0, 1}}),
                                             StoredVertex::Of({{1, 0, 0}}, {{1, 0}}, {{0, 0, 1}}),
                                             StoredVertex::Of({{0, 1, 0}}, {{0, 1}}, {{0, 0, 1}})};
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
  const size_t emptyBytes = residency.HeapBytes();
  CHECK(residency.Upload(kPage, page, frame.OriginEcef(), error, kSource),
        "a page accepts three native mesh parts");
  CHECK(renderer.PiecesStanding() == kPartCount && residency.Handles().size() == kPartCount &&
            residency.ValidateSources(error),
        "all native source pieces are independently resident");
  const uint64_t before = residency.Digest();
  const size_t loadedBytes = residency.HeapBytes();
  parts.back().Surface = PieceSurface::Registered(*first + kMissingMaterial);
  page.Digest = 2;
  CHECK(!residency.Upload(kPage, page, frame.OriginEcef(), error, kSource),
        "the third upload refuses after two successful candidate uploads");
  CHECK(renderer.PiecesStanding() == kPartCount && residency.Digest() == before &&
            residency.ValidateSources(error),
        "refusal releases every candidate part and preserves the published page");
  parts.back().Surface = PieceSurface::Registered(*first);
  CHECK(residency.Upload(kPage, page, frame.OriginEcef(), error, kSource) &&
            renderer.PiecesStanding() == kPartCount && residency.Digest() != before,
        "retry replaces the complete page without leaking old handles");
  CHECK(residency.Uploads() == 2 && residency.Refused() == 1,
        "page telemetry counts complete publications and refusals");
  const size_t beforeRetireBytes = residency.HeapBytes();
  residency.Retire(kPage);
  CHECK(renderer.PiecesStanding() == 0 && residency.Handles().empty(),
        "retirement releases all three native parts");
  CHECK(loadedBytes > emptyBytes &&
            beforeRetireBytes - residency.HeapBytes() >= kPartCount * sizeof(PieceHandle),
        "heap accounting includes resident handle arrays and their release");
  std::println("page-owner bytes empty={} initial={} before-retire={} retired={} handles={}",
               emptyBytes,
               loadedBytes,
               beforeRetireBytes,
               residency.HeapBytes(),
               kPartCount * sizeof(PieceHandle));
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
