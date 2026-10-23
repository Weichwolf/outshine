#include "PrismRenderer.h"
#include "UploadQueue.h"
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
#include <print>
#include <span>
#include <utility>
#include <vector>

namespace {
using namespace outshine;
using namespace outshine::Render;
using namespace outshine::Render::Native;
using namespace outshine::Test;
using namespace outshine::Test::Gpu;

class Observed final : public NativeDeviceProxy {
public:
  using NativeDeviceProxy::NativeDeviceProxy;
  bool Busy = false;
  bool RejectSubmit = false;
  unsigned Begins = 0;
  unsigned Submits = 0;
  unsigned Waits = 0;

  Result<std::unique_ptr<Commands>> Begin() override {
    ++Begins;
    if (Busy) {
      return std::unexpected(Error{.Kind = ErrorKind::Busy, .Message = "owned prism pressure"});
    }
    return Backing().Begin();
  }

  Result<Completion> Submit(std::unique_ptr<Commands> commands) override {
    ++Submits;
    if (std::exchange(RejectSubmit, false)) {
      return std::unexpected(Error{.Kind = ErrorKind::Driver, .Message = "owned prism failure"});
    }
    return Backing().Submit(std::move(commands));
  }

  Result<CompletionStatus> Wait(Completion completion, std::chrono::nanoseconds budget) override {
    ++Waits;
    return Backing().Wait(completion, budget);
  }
};

std::unique_ptr<Buffer> Readback(Device &device, Commands &commands, const Buffer &source) {
  const auto bytes = source.Description().Bytes;
  auto result = Require(device.CreateBuffer(
      {.Bytes = bytes, .Use = BufferUse::CopyDestination, .Placement = Memory::Readback}));
  Require(commands.Copy(source, *result, {.Bytes = bytes}));
  return result;
}

template <typename Value>
void Matches(Device &device, Buffer &buffer, std::span<const Value> expected) {
  const auto bytes = std::as_bytes(expected);
  const auto actual = Require(device.MapRead(buffer, {.Bytes = bytes.size()}));
  CHECK(actual.size() == bytes.size() &&
            std::memcmp(actual.data(), bytes.data(), bytes.size()) == 0,
        "recorded GPU data survives replacement and release byte-for-byte");
  Require(device.Unmap(buffer));
}

void FailedChanges(Observed &device,
                   UploadQueue &queue,
                   std::chrono::nanoseconds &budget,
                   PrismRenderer &renderer,
                   uint64_t id,
                   std::span<const PrismInstance> moved) {
  const auto oldRecords = renderer.Products()[0].Records;
  const auto oldBytes = renderer.HeldBytes();
  const auto submits = device.Submits;
  const auto begins = device.Begins;
  const auto waits = device.Waits;
  device.Busy = true;
  {
    const UploadQueue::WaitScope frame(budget, std::chrono::nanoseconds::zero());
    const auto busy = renderer.SetInstances(id, moved);
    CHECK(!busy && busy.error().Kind == ErrorKind::Busy && device.Begins == begins + 1 &&
              device.Waits == waits && device.Submits == submits,
          "frame pressure returns after one acquisition without a wait or partial publication");
  }
  device.Busy = false;
  device.RejectSubmit = true;
  CHECK(!renderer.SetInstances(id, moved), "a rejected upload preserves the previous product");
  CHECK(renderer.Products()[0].Records == oldRecords && renderer.Uploads().Submissions == 1 &&
            renderer.HeldBytes() == oldBytes,
        "failed attempts cannot replace CPU records or count accepted uploads");
  Require(renderer.SetInstances(id, moved));
  CHECK(renderer.Uploads().SampleBytes == 2 * sizeof(PrismSpan) &&
            renderer.Uploads().PatchBytes == 4 * sizeof(GpuPrismPatch) &&
            renderer.Uploads().Submissions == 2,
        "moving two patches transfers only their records and reuses immutable samples");
  const auto after = device.Submits;
  Require(renderer.SetInstances(id, moved));
  CHECK(device.Submits == after, "unchanged instances submit no GPU work");
  CHECK(&queue.Device() == &device, "the tested uploads use the owner's bounded queue");
}

void MemoryChanges(Observed &device,
                   PrismRenderer &renderer,
                   std::span<const PrismInstance> moved) {
  const auto before = renderer.HeldBytes();
  constexpr uint64_t sampleBytes = 2 * sizeof(PrismSpan);
  CHECK(before == sampleBytes + moved.size() * sizeof(GpuPrismPatch),
        "the live footprint includes both the sample buffer and every placed patch record");
  const auto id = Require(renderer.Place(renderer.Products()[0].Source, moved, 0));
  CHECK(renderer.HeldBytes() == 2 * before,
        "distinct products count their distinct buffers even when CPU samples are shared");
  Require(renderer.SetInstances(id, {}));
  CHECK(renderer.HeldBytes() == before + sampleBytes,
        "an empty placement retains and counts its reusable GPU sample buffer");
  Require(renderer.SetInstances(id, moved.first(1)));
  const auto partial = before + sampleBytes + sizeof(GpuPrismPatch);
  CHECK(renderer.HeldBytes() == partial, "the payload follows the actual resized record buffer");
  const auto submits = device.Submits;
  Require(renderer.SetInstances(id, moved.first(1)));
  CHECK(renderer.HeldBytes() == partial && device.Submits == submits,
        "unchanged placement neither uploads nor changes the reported payload");
  device.RejectSubmit = true;
  CHECK(!renderer.SetInstances(id, moved) && renderer.HeldBytes() == partial,
        "a rejected replacement preserves the previously published footprint");
  CHECK(renderer.Release(id) && renderer.HeldBytes() == before,
        "releasing one product leaves the other product's payload intact");
}

void Probe(Observed &device) {
  std::chrono::nanoseconds budget = std::chrono::seconds(5);
  UploadQueue queue(device, budget);
  PrismRenderer renderer;
  Require(renderer.Uses(device, &queue));
  const std::vector<PrismSpan> samples{{.Lower = -4, .Upper = 6}, {.Lower = 2, .Upper = 8}};
  auto source = std::make_shared<const PrismGrid>(PrismGrid{
      .Samples = std::make_shared<const std::vector<PrismSpan>>(samples),
      .Patches = {{.Columns = 1, .Rows = 1}, {.FirstSample = 1, .Columns = 1, .Rows = 1}}});
  std::array<PrismInstance, 2> instances{};
  instances[1].Patch = 1;
  const auto id = Require(renderer.Place(source, instances, 0));
  source.reset();
  CHECK(renderer.Products()[0].Source && renderer.Uploads().Submissions == 1,
        "one registration retains its immutable source and batches both transfers");
  const auto oldRecords = renderer.Products()[0].Records;
  auto commands = Require(queue.Begin());
  auto originalSamples = Readback(device, *commands, *renderer.Products()[0].Buffers[0]);
  auto originalRecords = Readback(device, *commands, *renderer.Products()[0].Buffers[1]);
  instances[0].Current[12] = 3;
  instances[1].Current[14] = -2;
  FailedChanges(device, queue, budget, renderer, id, instances);
  MemoryChanges(device, renderer, instances);
  CHECK(renderer.Release(id) && !renderer.Release(id), "a released resident cannot be reused");
  CHECK(renderer.Products().empty() && renderer.HeldBytes() == 0 &&
            !renderer.SetInstances(id, instances),
        "release removes the product and rejects subsequent mutation");
  const auto completion = Require(SubmitUpload(device, &queue, std::move(commands)));
  CHECK(Require(device.Wait(completion, std::chrono::seconds(5))) == CompletionStatus::Ready,
        "the readback finishes after all owning product wrappers have been released");
  Matches(device, *originalSamples, std::span<const PrismSpan>(samples));
  Matches(device, *originalRecords, std::span<const GpuPrismPatch>(oldRecords));
}
}

int main() {
  auto actual = Require(CreateDevice({.Validation = true}));
  Observed device(*actual);
  std::println("DEVICE {}", device.Limits().Name);
  Probe(device);
  return Report();
}
