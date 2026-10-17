#include "RuntimeScene.h"
#include "TerrainResidency.h"
#include "Check.h"
#include "test/harness/render/HeightPageReadback.h"
#include <SDL3/SDL.h>
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace {
using namespace outshine;
using namespace outshine::Test;
using namespace outshine::Test::Gpu;
namespace Native = Render::Native;
constexpr int kFrameSide = 32;
constexpr uint32_t kZoom = 14;
constexpr uint32_t kFirst = 8192;
constexpr float kOldHeight = 3;
constexpr float kChangedHeight = 7;
constexpr float kFinalHeight = 9;
constexpr size_t kPageBytes = Render::GroundLattice::kPageNodes * sizeof(float);
constexpr size_t kTarget = Render::kHeightPageBatchMost + 1;
constexpr size_t kFinishSteps = 8;
constexpr LongitudeLatitude kRemovedPoint{.LongitudeDeg = 0.2427, .LatitudeDeg = -0.001};
constexpr LongitudeLatitude kChangedPoint{.LongitudeDeg = 0.2207, .LatitudeDeg = -0.001};

Patchwork Pages(size_t first, size_t count, float height) {
  Patchwork patch;
  for (size_t at = first; at < first + count; ++at) {
    patch.Sheets.push_back(
        {.Tile = {.Zoom = kZoom, .X = kFirst + static_cast<uint32_t>(at), .Y = kFirst},
         .Nodes = std::vector<float>(Render::GroundLattice::kPageNodes, height),
         .Side = Render::GroundLattice::kSide,
         .Postings = Render::GroundLattice::kSide});
  }
  return patch;
}

bool Finish(Render::SceneRenderer &renderer,
            TerrainResidency &residency,
            const Patchwork &patch,
            const TangentFrame &frame) {
  std::string error;
  for (size_t step = 0; step < kFinishSteps; ++step) {
    const auto result = residency.AdvancePublish(patch, frame, Render::kHeightPageBatchMost);
    CHECK(result.has_value(), "the remaining bounded publication has no permanent error");
    if (!result) { return false; }
    if (*result) { return true; }
    CHECK(renderer.Settle(error), "test-only completion permits the next bounded upload step");
  }
  return false;
}

void Case(Render::SceneRenderer &renderer, UploadFailure &failure) {
  const auto frame = TangentFrame::At({});
  auto original = Pages(10, 2, kOldHeight);
  std::ranges::reverse(original.Sheets);
  TerrainResidency residency;
  residency.Into(&renderer);
  std::string error;
  CHECK(residency.BeginPublish(original, error) && Finish(renderer, residency, original, frame),
        "the original complete terrain is published");
  CHECK(renderer.Settle(error), "original uploads finish before fault injection");
  auto *const oldAtlas = failure.LastHeightAtlas;
  auto changedRegion = failure.LastHeightRegion;
  const auto removedHeight = residency.HeightMAt(kRemovedPoint);
  const auto changedHeight = residency.HeightMAt(kChangedPoint);
  CHECK(removedHeight && changedHeight, "both original tiles support real height queries");
  const auto triangles = renderer.GroundLatticeTriangles();
  const auto bytes = renderer.HeightPageSourceBytes();
  auto replacement = Pages(0, kTarget + 1, kChangedHeight);
  replacement.Sheets.erase(replacement.Sheets.begin() + 11);
  CHECK(residency.BeginPublish(replacement, error), "a changed and expanded patchwork begins");
  CHECK(residency.HeightMAt(kRemovedPoint) == removedHeight &&
            renderer.HeightPageSourceBytes() == bytes,
        "beginning a replacement does not release still-published old pages");
  failure.Kind = Native::ErrorKind::Busy;
  failure.Point = UploadFailurePoint::Copy;
  failure.Skip = 1;
  failure.Armed = true;
  const auto waits = failure.Waits;
  const auto accepted = failure.AcceptedSubmissions;
  const auto refused = residency.AdvancePublish(replacement, frame, kTarget);
  CHECK(refused && !*refused && !failure.Armed && failure.Waits == waits,
        "a mid-group refusal defers without waiting inside the publication step");
  CHECK(failure.AcceptedSubmissions == accepted && residency.Standing() == 2 &&
            renderer.HeightPageSourceBytes() == bytes &&
            renderer.GroundLatticeTriangles() == triangles,
        "unsubmitted group work exposes no cursor progress, payload or partial topology");
  CHECK(residency.HeightMAt(kRemovedPoint) == removedHeight &&
            residency.HeightMAt(kChangedPoint) == changedHeight,
        "rejected replacement retains removed and changed old tile values");
  const auto uploads = failure.UploadBufferRequests;
  const auto first = residency.AdvancePublish(replacement, frame, kTarget);
  CHECK(first && !*first && failure.UploadBufferRequests == uploads + 1 &&
            failure.AcceptedSubmissions == accepted + 1,
        "a larger requested step still accepts at most one packed 48-page group");
  CHECK(residency.Standing() == Render::kHeightPageBatchMost + 1 &&
            renderer.HeightPageSourceBytes() == (Render::kHeightPageBatchMost + 2) * kPageBytes &&
            renderer.GroundLatticeTriangles() == triangles,
        "accepted pages are retained while final topology and removed-page retirement wait");
  CHECK(residency.HeightMAt(kRemovedPoint) == removedHeight,
        "an old tile scheduled for removal remains valid before complete publication");
  CHECK(renderer.Settle(error), "accepted writes finish before inspecting the old visible page");
  const auto oldValues = ReadHeightPage(failure, *oldAtlas, changedRegion);
  CHECK(std::ranges::all_of(oldValues, [](float value) { return value == kOldHeight; }),
        "accepted partial replacement retains every texel of the still-published changed page");
  CHECK(renderer.Settle(error) && Finish(renderer, residency, replacement, frame),
        "retry consumes the remaining page and publishes the full unchanged demand");
  CHECK(residency.Standing() == kTarget && residency.Instances() == kTarget &&
            renderer.HeightPageSourceBytes() == kTarget * kPageBytes &&
            renderer.GroundLatticeTriangles() == kTarget * (Render::GroundLattice::kIndices / 3u),
        "completion publishes all demanded tiles and retires precisely the removed tile");
  CHECK(!residency.HeightMAt(kRemovedPoint) && residency.HeightMAt(kChangedPoint) != changedHeight,
        "height queries switch from old removal coverage to accepted replacement data");
  const auto prior = residency.HeightMAt(kChangedPoint);
  for (auto &sheet : replacement.Sheets) { sheet.Nodes.assign(sheet.Nodes.size(), kFinalHeight); }
  CHECK(renderer.Settle(error) && residency.BeginPublish(replacement, error),
        "prior GPU work finishes before a permanent submission refusal");
  failure.Point = UploadFailurePoint::Submit;
  failure.Kind = Native::ErrorKind::Driver;
  failure.Armed = true;
  const auto broken = residency.AdvancePublish(replacement, frame, kTarget);
  CHECK(!broken && broken.error() == failure.Message && !failure.Armed &&
            residency.HeightMAt(kChangedPoint) == prior &&
            renderer.HeightPageSourceBytes() == kTarget * kPageBytes,
        "a real submission error preserves the previous complete terrain and its data");
  CHECK(Finish(renderer, residency, replacement, frame) &&
            residency.HeightMAt(kChangedPoint) != prior,
        "the unchanged demand can retry successfully after the permanent error");
  residency.Clear();
}
}

int main() {
  CHECK(SDL_Init(SDL_INIT_VIDEO), "video initializes");
  {
    Core::Declaration declaration;
    declaration.SurfaceWidthPx = declaration.SurfaceHeightPx = kFrameSide;
    declaration.Outputs = {"surface"};
    UploadFailure failure;
    Render::SceneRenderer renderer(
        {.Context = &failure, .Create = NativeUploadFaultDevice::Create});
    std::unique_ptr<Core::RuntimeScene> scene;
    std::string error;
    CHECK(Core::RuntimeScene::Open(renderer, declaration, nullptr, scene, error), "world opens");
    if (scene && renderer.Settle(error)) { Case(renderer, failure); }
  }
  SDL_Quit();
  return Report();
}
