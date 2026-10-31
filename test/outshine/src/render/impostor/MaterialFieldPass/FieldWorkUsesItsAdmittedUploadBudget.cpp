#include "MaterialFieldPass.h"
#include "UploadQueue.h"
#include "test/harness/render/NativeDeviceProxy.h"
#include "test/harness/render/NativeGpuCheck.h"
#include "test/harness/render/NativeMaterialField.h"
#include "Check.h"
#include <SDL3/SDL.h>
#include <chrono>
#include <cstddef>
#include <expected>
#include <memory>
#include <string_view>

namespace {
using namespace outshine::Render;
using namespace outshine::Render::Native;
using namespace outshine::Test;
using namespace outshine::Test::Gpu;

class Observed final : public NativeDeviceProxy {
public:
  using NativeDeviceProxy::NativeDeviceProxy;
  enum class WaitResult { Forward, Pending, Failure };
  WaitResult Reply = WaitResult::Forward;
  size_t BusyBegins = 0, Waits = 0, Allocations = 0;
  std::chrono::nanoseconds LastBudget{};

  Result<std::unique_ptr<Commands>> Begin() override {
    if (BusyBegins > 0) {
      --BusyBegins;
      return std::unexpected(Error{.Kind = ErrorKind::Busy, .Message = "owned acquire pressure"});
    }
    return Backing().Begin();
  }

  Result<CompletionStatus> Wait(Completion completion, std::chrono::nanoseconds budget) override {
    ++Waits;
    LastBudget = budget;
    if (Reply == WaitResult::Pending) { return CompletionStatus::Pending; }
    if (Reply == WaitResult::Failure) {
      return std::unexpected(Error{.Kind = ErrorKind::Driver, .Message = "owned wait failure"});
    }
    return Backing().Wait(completion, budget);
  }

  Result<std::unique_ptr<Buffer>> CreateBuffer(BufferDescription description,
                                               std::string_view label = {}) override {
    ++Allocations;
    return Backing().CreateBuffer(description, label);
  }

  Result<std::unique_ptr<Texture>> CreateTexture(TextureDescription description,
                                                 std::string_view label = {}) override {
    ++Allocations;
    return Backing().CreateTexture(description, label);
  }
};

void Drain(Observed &device, MaterialFieldPass &pass) {
  if (const auto last = pass.LastSubmission()) {
    CHECK(Require(device.Wait(*last, std::chrono::seconds(5))) == CompletionStatus::Ready,
          "the explicit offline driver drains accepted GPU work");
  }
  Require(pass.Collect());
}

void CheckAdmission(Observed &device,
                    MaterialFieldPass &pass,
                    std::chrono::nanoseconds &budget,
                    const auto &operation) {
  const auto before = pass.LastSubmission();
  CHECK(before, "a held publication supplies the queue's last completion");
  if (!before) { return; }
  const auto allocations = device.Allocations;
  auto waits = device.Waits;
  budget = std::chrono::nanoseconds::zero();
  device.BusyBegins = 1;
  const auto immediate = operation();
  CHECK(!immediate && immediate.error().Kind == ErrorKind::Busy && device.Waits == waits &&
            device.Allocations == allocations,
        "frame-budget acquisition stays nonblocking and allocates nothing on Busy");
  budget = std::chrono::seconds(2);
  for (const auto reply : {Observed::WaitResult::Pending, Observed::WaitResult::Failure}) {
    device.Reply = reply;
    device.BusyBegins = 1;
    waits = device.Waits;
    const auto deferred = operation();
    const auto expected =
        reply == Observed::WaitResult::Pending ? ErrorKind::Busy : ErrorKind::Driver;
    CHECK(!deferred && deferred.error().Kind == expected && device.Waits == waits + 1 &&
              device.LastBudget == budget && device.Allocations == allocations,
          "pending or failed bounded waits retain the native failure before allocation");
    const auto held = pass.LastSubmission();
    CHECK(held && held->Device == before->Device && held->Serial == before->Serial,
          "a deferred field leaves its last accepted publication untouched");
  }
  device.Reply = Observed::WaitResult::Forward;
  device.BusyBegins = 2;
  waits = device.Waits;
  const auto repeated = operation();
  CHECK(!repeated && repeated.error().Kind == ErrorKind::Busy && device.Waits == waits + 1 &&
            device.BusyBegins == 0 && device.Allocations == allocations,
        "offline acquisition retries once rather than spinning while admission stays Busy");
  device.BusyBegins = 1;
  waits = device.Waits;
  const auto accepted = operation();
  CHECK(accepted && device.Waits == waits + 1 && device.LastBudget == budget,
        "Ready within the admitted offline budget retries and completes the real field operation");
  Drain(device, pass);
}

void Exercise() {
  auto native = Require(CreateDevice({.Validation = true}));
  Observed device(*native);
  std::chrono::nanoseconds budget = std::chrono::seconds(2);
  UploadQueue uploads(device, budget);
  MaterialFieldPass pass(device, &uploads);
  Require(pass.Configure());
  const auto input = PreparedMaterialField({.4f, .5f, .6f, 1});
  device.BusyBegins = 1;
  const auto noPrevious = pass.Prepare(input);
  CHECK(!noPrevious && noPrevious.error().Kind == ErrorKind::Busy && device.Waits == 0 &&
            device.Allocations == 0 && !pass.LastSubmission(),
        "an unseeded queue cannot invent a completion to wait for");
  const auto source = Require(pass.Prepare(input));
  Drain(device, pass);
  CheckAdmission(device, pass, budget, [&] { return pass.Prepare(input); });
  MaterialFieldView view;
  view.Camera.Kind = CameraKind::Orthographic;
  view.Camera.EyeM = {{0, 0, 2}};
  view.Camera.XMagM = view.Camera.YMagM = 1;
  view.Camera.ZNearM = .1;
  view.Camera.ZFarM = 10;
  view.Target = {.WidthPx = 16, .HeightPx = 16};
  CheckAdmission(device, pass, budget, [&] { return pass.Relight(*source, view); });
}
}

int main() {
  CHECK(SDL_Init(SDL_INIT_VIDEO), "video initializes for native admission checks");
  Exercise();
  SDL_Quit();
  return Report();
}
