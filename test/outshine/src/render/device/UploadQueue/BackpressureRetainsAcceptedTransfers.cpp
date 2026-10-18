#include "UploadQueue.h"
#include "ReadbackTransfer.h"
#include "test/harness/render/NativeDeviceProxy.h"
#include "test/harness/render/NativeGpuCheck.h"
#include "Check.h"
#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <expected>
#include <memory>
#include <span>
#include <string_view>
#include <utility>

namespace {
using namespace outshine::Render;
using namespace outshine::Render::Native;
using namespace outshine::Test;
using namespace outshine::Test::Gpu;

class Observed final : public NativeDeviceProxy {
public:
  using NativeDeviceProxy::NativeDeviceProxy;
  unsigned Busy = 0, Begins = 0, Waits = 0, Submits = 0;
  bool DriverFailure = false, WaitFailure = false, ForeverBusy = false;

  Result<std::unique_ptr<Commands>> Begin() override {
    ++Begins;
    if (DriverFailure) {
      return std::unexpected(Error{.Kind = ErrorKind::Driver, .Message = "owned acquire failure"});
    }
    if (ForeverBusy || Busy > 0) {
      if (Busy > 0) { --Busy; }
      return std::unexpected(
          Error{.Kind = ErrorKind::Busy, .Message = "owned temporary backpressure"});
    }
    return Backing().Begin();
  }

  Result<Completion> Submit(std::unique_ptr<Commands> commands) override {
    ++Submits;
    return Backing().Submit(std::move(commands));
  }

  Result<CompletionStatus> Wait(Completion completion, std::chrono::nanoseconds budget) override {
    ++Waits;
    if (WaitFailure) {
      return std::unexpected(Error{.Kind = ErrorKind::Driver, .Message = "owned wait failure"});
    }
    return Backing().Wait(completion, budget);
  }
};

void Probe(Observed &device) {
  constexpr std::array<uint32_t, 4> expected{17, 31, 47, 61};
  std::chrono::nanoseconds budget{};
  UploadQueue queue(device, budget);
  auto source = Require(device.CreateBuffer(
      {.Bytes = sizeof(expected), .Use = BufferUse::CopySource, .Placement = Memory::Upload}));
  auto target = Require(device.CreateBuffer(
      {.Bytes = sizeof(expected), .Use = BufferUse::CopyDestination | BufferUse::CopySource}));
  const auto mapped = Require(device.MapWrite(*source, {.Bytes = sizeof(expected)}));
  std::memcpy(mapped.data(), expected.data(), sizeof(expected));
  Require(device.Unmap(*source));
  auto commands = Require(BeginUpload(device, &queue));
  Require(commands->Copy(*source, *target, {.Bytes = sizeof(expected)}));
  const auto accepted = Require(SubmitUpload(device, &queue, std::move(commands)));
  CHECK(device.Submits == 1, "the data transfer is submitted exactly once");
  {
    const UploadQueue::WaitScope offline(budget, std::chrono::seconds(5));
    device.Busy = 1;
    auto retry = Require(queue.Begin());
    CHECK(device.Waits == 1 && device.Submits == 1,
          "setup waits on its accepted completion without replaying the data transfer");
    retry.reset();
    const auto waits = device.Waits;
    const auto begins = device.Begins;
    {
      const UploadQueue::WaitScope frame(budget, std::chrono::nanoseconds::zero());
      device.Busy = 1;
      const auto busy = queue.Begin();
      CHECK(!busy && busy.error().Kind == ErrorKind::Busy && device.Begins == begins + 1 &&
                device.Waits == waits,
            "the frame path returns Busy after one acquire and never waits");
    }
    CHECK(budget == std::chrono::seconds(5),
          "leaving the frame restores the explicit outer setup budget");
    device.Busy = 1;
    device.WaitFailure = true;
    const auto failed = queue.Begin();
    CHECK(!failed && failed.error().Message == "owned wait failure",
          "setup returns an owned completion error without losing the prior transfer");
    device.WaitFailure = false;
    device.DriverFailure = true;
    const auto before = device.Begins;
    const auto failedAcquire = queue.Begin();
    CHECK(!failedAcquire && failedAcquire.error().Message == "owned acquire failure" &&
              device.Begins == before + 1,
          "non-Busy failures propagate without automatic retry");
    device.DriverFailure = false;
  }
  CHECK(budget == std::chrono::nanoseconds::zero(),
        "setup restores the default nonblocking budget");
  CHECK(Require(device.Wait(accepted, std::chrono::seconds(5))) == CompletionStatus::Ready,
        "the original transfer completion stays valid after all retries and failures");
  auto read = Require(ReadbackTransfer::Start(device, *target, {.Bytes = sizeof(expected)}));
  const auto bytes = Require(read.Wait(device, std::chrono::seconds(5)));
  CHECK(bytes && bytes->size() == sizeof(expected) &&
            std::memcmp(bytes->data(), expected.data(), sizeof(expected)) == 0,
        "the actual native destination preserves every submitted source byte");
  Observed other(static_cast<Device &>(device));
  const auto begins = device.Begins;
  CHECK(!BeginUpload(other, &queue) && device.Begins == begins,
        "an upload cannot acquire through another device's queue");
  device.ForeverBusy = true;
  {
    const UploadQueue::WaitScope bounded(budget, std::chrono::milliseconds(1));
    const auto expired = queue.Begin();
    CHECK(!expired && expired.error().Kind == ErrorKind::Busy,
          "persistent backpressure returns recoverable Busy after one bounded wait and retry");
  }
  device.ForeverBusy = false;
}
}

int main() {
  auto actual = Require(CreateDevice({.Validation = true}));
  Observed device(*actual);
  Probe(device);
  return Report();
}
