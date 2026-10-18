#include "Check.h"
#include "Gpu.h"
#include "SubjectDraw.h"
#include "UploadQueue.h"
#include "test/harness/render/NativeDeviceProxy.h"
#include "test/harness/render/NativeGpuCheck.h"
#include <array>
#include <chrono>
#include <expected>
#include <memory>
#include <utility>

namespace {
using namespace outshine::Render::Native;

struct Observed final : outshine::Test::Gpu::NativeDeviceProxy {
  explicit Observed(Device &device) : NativeDeviceProxy(device) {}

  bool Busy = false;
  unsigned Waits = 0;

  Result<std::unique_ptr<Commands>> Begin() override {
    if (std::exchange(Busy, false)) {
      return std::unexpected(Error{.Kind = ErrorKind::Busy, .Message = "height-page admission"});
    }
    return Backing().Begin();
  }

  Result<CompletionStatus> Wait(Completion completion, std::chrono::nanoseconds budget) override {
    ++Waits;
    return Backing().Wait(completion, budget);
  }
};
}

int main() {
  using namespace outshine::Render;
  using namespace outshine::Test;
  using outshine::Test::Gpu::Require;
  auto actual = Require(Native::CreateDevice({.Validation = true}));
  Observed device(*actual);
  std::chrono::nanoseconds budget = std::chrono::seconds(5);
  UploadQueue uploads(device, budget);
  outshine::Render::Gpu gpu;
  gpu.Device = &device;
  gpu.HdrFormat = Native::Format::Rgba16Float;
  gpu.SurfaceFormat = Native::Format::Rgba8Unorm;
  gpu.Uploads = &uploads;
  gpu.Width = gpu.Height = 32;
  CHECK(gpu.SceneColours.Add(Resource::SceneLinear),
        "the configured subject has its colour target");
  SubjectDraw draw;
  Require(draw.Configure(gpu));
  const auto marker = Require(uploads.Submit(Require(uploads.Begin())));
  CHECK(Require(actual->Wait(marker, std::chrono::seconds(5))) == Native::CompletionStatus::Ready,
        "the admission retry has an actual accepted completion");
  std::array<float, GroundLattice::kPageNodes> heights{};
  constexpr float heightM = 3.5f;
  heights.fill(heightM);
  const auto generation = draw.Ground().Generation();
  const auto waited = device.Waits;
  {
    const UploadQueue::WaitScope frame(budget, std::chrono::nanoseconds::zero());
    device.Busy = true;
    const auto blocked = draw.Ground().PlacePage(heights);
    CHECK(!blocked && blocked.error().Kind == Native::ErrorKind::Busy && device.Waits == waited,
          "a configured terrain page never waits for admission inside the frame");
    CHECK(draw.Ground().PagesStanding() == 0 && draw.Ground().Generation() == generation,
          "rejected page admission publishes neither a page nor a generation");
  }
  device.Busy = true;
  const auto page = draw.Ground().PlacePage(heights);
  CHECK(page && *page == 0 && device.Waits == waited + 1 && !device.Busy,
        "the same configured terrain consumer resumes through the bounded offline upload queue");
  CHECK(draw.Ground().PagesStanding() == 1 && draw.Ground().Generation() == generation + 1,
        "the resumed upload publishes exactly one page without regenerating the input");
  return Report();
}
