#include "RuntimeScene.h"
#include "SceneRenderer.h"
#include "Check.h"
#include "test/harness/render/HeightPageReadback.h"
#include <SDL3/SDL.h>
#include <algorithm>
#include <array>
#include <chrono>
#include <cstddef>
#include <memory>
#include <span>
#include <string>
#include <vector>

namespace {
using namespace outshine;
using namespace outshine::Test;
using namespace outshine::Test::Gpu;
namespace Native = Render::Native;
constexpr size_t kBatch = 4;
constexpr int kFrameSide = 32;
constexpr float kOldHeight = 3;
constexpr float kHeightStep = 0.125f;

Native::Result<std::unique_ptr<Native::Device>> CreateValidated(void *context,
                                                                Native::DeviceOptions options) {
  options.Validation = true;
  return NativeUploadFaultDevice::Create(context, options);
}

bool SameBits(std::span<const float> a, std::span<const float> b) {
  return std::ranges::equal(std::as_bytes(a), std::as_bytes(b));
}

void Case(Render::SceneRenderer &renderer, UploadFailure &failure) {
  std::string error;
  std::vector<float> old(Render::GroundLattice::kPageNodes, kOldHeight);
  const auto original = Require(renderer.PlaceHeightPage(old));
  auto *const oldAtlas = failure.LastHeightAtlas;
  const auto oldRegion = failure.LastHeightRegion;
  CHECK(oldAtlas != nullptr, "the fixture records the actual R32 atlas transfer");
  if (oldAtlas == nullptr) { return; }
  CHECK(SameBits(ReadHeightPage(failure, *oldAtlas, oldRegion), old),
        "the complete original page is present on the GPU");
  const auto bytes = renderer.HeightPageSourceBytes();
  std::array<std::vector<float>, kBatch> values;
  std::array<Render::HeightPageView, kBatch> nodes;
  for (size_t at = 0; at < kBatch; ++at) {
    values[at].resize(Render::GroundLattice::kPageNodes);
    for (size_t node = 0; node < values[at].size(); ++node) {
      values[at][node] = static_cast<float>(at + 1) + static_cast<float>(node) * kHeightStep;
    }
    nodes[at] = values[at];
  }
  failure.Point = UploadFailurePoint::Copy;
  failure.Skip = 1;
  failure.Kind = Native::ErrorKind::Busy;
  failure.Armed = true;
  const auto refused = renderer.PlaceHeightPages(nodes);
  CHECK(!refused && refused.error().Kind == Native::ErrorKind::Busy && !failure.Armed,
        "a refusal after a recorded page rejects the complete group");
  CHECK(renderer.HasHeightPage(original) && renderer.HeightPageSlots() == 1 &&
            renderer.HeightPageSourceBytes() == bytes,
        "a rejected group publishes no handles or retained payload");
  CHECK(SameBits(ReadHeightPage(failure, *oldAtlas, oldRegion), old),
        "recorded but unsubmitted copies preserve every old R32 value");
  const auto uploads = failure.UploadBufferRequests;
  const auto submissions = failure.AcceptedSubmissions;
  const auto placed = Require(renderer.PlaceHeightPages(nodes));
  CHECK(placed.size() == kBatch && failure.UploadBufferRequests == uploads + 1 &&
            failure.AcceptedSubmissions == submissions + 1,
        "the accepted group uses one staging buffer and one submission");
  CHECK(std::ranges::all_of(placed, [&](auto page) { return renderer.HasHeightPage(page); }),
        "all returned generations name accepted native pages");
  CHECK(SameBits(ReadHeightPage(failure, *failure.LastHeightAtlas, failure.LastHeightRegion),
                 values.back()),
        "the last packed offset preserves its complete distinct page payload");
  std::array<Render::HeightPageView, Render::kHeightPageBatchMost + 1> oversized;
  oversized.fill(old);
  const auto accepted = failure.AcceptedSubmissions;
  CHECK(!renderer.PlaceHeightPages(oversized) && failure.AcceptedSubmissions == accepted,
        "an oversized group rejects before submission");
  CHECK(Require(renderer.PlaceHeightPages({})).empty() && failure.AcceptedSubmissions == accepted,
        "an empty group creates no GPU work");
  std::array<Render::HeightPageView, Render::kHeightPageBatchMost> fill;
  fill.fill(old);
  while (renderer.HeightPageSlots() < Render::GroundLattice::kPagesPerLayer - 1u) {
    const size_t count = std::min(
        fill.size(), Render::GroundLattice::kPagesPerLayer - 1u - renderer.HeightPageSlots());
    static_cast<void>(Require(
        RetryOffline([&] { return renderer.PlaceHeightPages(std::span(fill).first(count)); },
                     std::chrono::seconds(5))));
  }
  CHECK(renderer.Settle(error), "prior accepted writes finish before the growth fault");
  failure.Point = UploadFailurePoint::Submit;
  failure.Kind = Native::ErrorKind::Driver;
  failure.Armed = true;
  const auto growth = renderer.PlaceHeightPages(nodes);
  CHECK(!growth && growth.error().Message == failure.Message && !failure.Armed,
        "rejected atlas growth and packed writes report their submission cause");
  CHECK(renderer.HasHeightPage(original) &&
            renderer.HeightPageSlots() == Render::GroundLattice::kPagesPerLayer - 1u,
        "failed growth exposes no partial slot commit");
  CHECK(SameBits(ReadHeightPage(failure, *oldAtlas, oldRegion), old),
        "failed growth retains the original physical atlas and values");
  static_cast<void>(Require(renderer.PlaceHeightPages(nodes)));
  CHECK(SameBits(ReadHeightPage(failure, *failure.LastHeightAtlas, oldRegion), old),
        "accepted growth copies the old layer without changing its values");
  CHECK(failure.LastHeightRegion.Layer == 1 &&
            SameBits(ReadHeightPage(failure, *failure.LastHeightAtlas, failure.LastHeightRegion),
                     values.back()),
        "the packed group crosses a layer boundary with exact source offsets");
}
}

int main() {
  CHECK(SDL_Init(SDL_INIT_VIDEO), "video initializes");
  {
    Core::Declaration declaration;
    declaration.SurfaceWidthPx = declaration.SurfaceHeightPx = kFrameSide;
    declaration.Outputs = {"surface"};
    UploadFailure failure;
    Render::SceneRenderer renderer({.Context = &failure, .Create = CreateValidated});
    std::unique_ptr<Core::RuntimeScene> scene;
    std::string error;
    CHECK(Core::RuntimeScene::Open(renderer, declaration, nullptr, scene, error), "world opens");
    if (scene && renderer.Settle(error)) { Case(renderer, failure); }
  }
  SDL_Quit();
  return Report();
}
