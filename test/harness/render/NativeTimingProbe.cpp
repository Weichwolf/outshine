#include "NativeGpuCheck.h"
#include "Check.h"
#include <array>
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <limits>
#include <memory>
#include <print>
#include <thread>
#include <utility>

namespace {
using namespace outshine::Render::Native;
using namespace outshine::Test;
using namespace outshine::Test::Gpu;
constexpr uint32_t kLongSteps = 20000000;
constexpr uint32_t kTruncatedDispatches = 130;
constexpr uint32_t kRetainedSubmissions = 8;
constexpr uint32_t kMultiplier = 1664525;
constexpr uint32_t kIncrement = 1013904223;
constexpr double kNanosecondsPerMillisecond = 1'000'000.0;

uint32_t Expected(uint32_t steps) {
  uint32_t value = 7;
  for (uint32_t at = 0; at < steps; ++at) {
    value = (value ^ (value >> 13u)) * kMultiplier + kIncrement;
  }
  return value;
}

void Complete(Device &device, Completion done) {
  CHECK(Require(device.Wait(done, std::chrono::seconds(5))) == CompletionStatus::Ready,
        "bounded offline wait completes the real GPU workload");
}

void CheckRows(const GpuTimingResults &result, uint64_t count, bool truncated) {
  CHECK(result.Status == CompletionStatus::Ready, "completed timing has Ready status");
  CHECK(result.DispatchCount == count && result.Truncated == truncated,
        "timing reports actual dispatch count and bounded omissions");
  CHECK(result.Dispatches.size() == std::min<uint64_t>(count, 128),
        "at most 128 real dispatch pairs are retained per submission");
  for (size_t index = 0; index < result.Dispatches.size(); ++index) {
    const auto &sample = result.Dispatches[index];
    CHECK(sample.Ordinal == index && sample.Available,
          "each completed dispatch has its own ordinal and available pair");
    CHECK(sample.ValidBits > 0 && sample.ValidBits <= 64 &&
              std::isfinite(sample.NanosecondsPerTick) && sample.NanosecondsPerTick > 0,
          "device tick period and queue timestamp precision are explicit");
  }
}

Completion Empty(Device &device) {
  auto commands = Require(device.Begin());
  const auto done = Require(device.Submit(std::move(commands)));
  Complete(device, done);
  return done;
}

void Disabled() {
  auto device = Require(CreateDevice({.Validation = true}));
  const auto done = Empty(*device);
  auto result = device->GpuTimings(done);
  CHECK(!result && result.error().Kind == ErrorKind::Unsupported,
        "default devices do not silently enable GPU query work");
}

struct Workload {
  std::unique_ptr<ComputePipeline> Pipeline;
  std::unique_ptr<Buffer> Values;
  std::unique_ptr<Buffer> Readback;
};

Workload Prepare(Device &device, const char *shader, uint32_t steps) {
  Workload work;
  work.Pipeline = Require(device.CreateComputePipeline(Spirv(shader)));
  auto commands = Require(device.Begin());
  const std::array<uint32_t, 2> values{7, steps};
  work.Values = Upload(device, *commands, values);
  work.Readback = Require(device.CreateBuffer(
      {.Bytes = sizeof(values), .Use = BufferUse::CopyDestination, .Placement = Memory::Readback}));
  Complete(device, Require(device.Submit(std::move(commands))));
  return work;
}

Completion Execute(Device &device, Workload &work, uint32_t dispatches) {
  auto commands = Require(device.Begin());
  const std::array bindings{Storage(0, 0, *work.Values)};
  for (uint32_t at = 0; at < dispatches; ++at) {
    Require(commands->Dispatch(*work.Pipeline, bindings, {.X = 1, .Y = 1, .Z = 1}));
  }
  Require(commands->Copy(*work.Values, *work.Readback, {.Bytes = 2 * sizeof(uint32_t)}));
  return Require(device.Submit(std::move(commands)));
}

void CheckValues(Device &device, Workload &work, uint32_t steps, uint32_t repetitions) {
  const auto bytes = Require(device.MapRead(*work.Readback, {.Bytes = 2 * sizeof(uint32_t)}));
  std::array<uint32_t, 2> values{};
  std::memcpy(values.data(), bytes.data(), sizeof(values));
  Require(device.Unmap(*work.Readback));
  CHECK(values[0] == Expected(steps * repetitions) && values[1] == steps,
        "timestamp commands preserve dependent GPU arithmetic and readback bytes");
}

Completion PendingAndReady(Device &device, const char *shader) {
  auto work = Prepare(device, shader, kLongSteps);
  const auto done = Execute(device, work, 1);
  const auto pending = Require(device.GpuTimings(done));
  CHECK(pending.Status == CompletionStatus::Pending && pending.Dispatches.empty() &&
            pending.DispatchCount == 0 && !pending.Truncated,
        "live GPU work returns Pending without partial or invented timestamps");
  Complete(device, done);
  const auto ready = Require(device.GpuTimings(done));
  CheckRows(ready, 1, false);
  const auto &sample = ready.Dispatches.front();
  const uint64_t mask = std::numeric_limits<uint64_t>::max() >> (64 - sample.ValidBits);
  const auto ticks = (sample.End - sample.Begin) & mask;
  CHECK(ticks > 0, "the actual dependent integer workload has positive GPU elapsed ticks");
  std::println("native_dispatch begin={} end={} ns_per_tick={} valid_bits={} elapsed_ms={}",
               sample.Begin,
               sample.End,
               sample.NanosecondsPerTick,
               sample.ValidBits,
               static_cast<double>(ticks) * sample.NanosecondsPerTick / kNanosecondsPerMillisecond);
  auto copy = Require(device.GpuTimings(done));
  copy.Dispatches.front().Begin = 0;
  const auto again = Require(device.GpuTimings(done));
  CHECK(again.Dispatches.front().Begin == sample.Begin &&
            again.Dispatches.front().End == sample.End,
        "retrieval is repeatable and caller mutation cannot change retained results");
  CheckValues(device, work, kLongSteps, 1);
  return done;
}

void Bounded(Device &device, char *const *shaders, Completion oldest) {
  auto work = Prepare(device, shaders[0], 1);
  auto emptyPipeline = Require(device.CreateComputePipeline(Spirv(shaders[1])));
  auto commands = Require(device.Begin());
  const std::array bindings{Storage(0, 0, *work.Values)};
  Require(commands->Dispatch(*work.Pipeline, bindings, {.X = 1, .Y = 1, .Z = 1}));
  for (uint32_t at = 1; at < kTruncatedDispatches; ++at) {
    Require(commands->Dispatch(*emptyPipeline, {}, {.X = 1, .Y = 1, .Z = 1}));
  }
  Require(commands->Copy(*work.Values, *work.Readback, {.Bytes = 2 * sizeof(uint32_t)}));
  const auto done = Require(device.Submit(std::move(commands)));
  Complete(device, done);
  CheckRows(Require(device.GpuTimings(done)), kTruncatedDispatches, true);
  CheckValues(device, work, 1, 1);
  for (uint32_t at = 0; at < kRetainedSubmissions; ++at) {
    const auto empty = Empty(device);
    CheckRows(Require(device.GpuTimings(empty)), 0, false);
  }
  const auto expired = device.GpuTimings(oldest);
  CHECK(!expired && expired.error().Kind == ErrorKind::Invalid,
        "the ninth newer result expires the old completion explicitly");
  const auto justExpired = device.GpuTimings(done);
  CHECK(!justExpired && justExpired.error().Kind == ErrorKind::Invalid,
        "retention counts submissions without dispatches and remains bounded to eight");
}

void Ownership(Device &device, const char *shader, Completion done) {
  auto foreign = done;
  ++foreign.Device;
  const auto rejected = device.GpuTimings(foreign);
  CHECK(!rejected && rejected.error().Kind == ErrorKind::Invalid,
        "timing retrieval rejects foreign completion ownership");
  bool wrongThread = false;
  std::thread other([&] {
    auto result = device.GpuTimings(done);
    wrongThread = !result && result.error().Kind == ErrorKind::Invalid;
  });
  other.join();
  CHECK(wrongThread, "timing query access preserves the device owner thread contract");
  auto work = Prepare(device, shader, 1);
  auto abandoned = Require(device.Begin());
  const std::array bindings{Storage(0, 0, *work.Values)};
  Require(abandoned->Dispatch(*work.Pipeline, bindings, {.X = 1, .Y = 1, .Z = 1}));
  abandoned.reset();
  CheckRows(Require(device.GpuTimings(Empty(device))), 0, false);
}

}

int main(int argc, char **argv) {
  if (argc != 3) { return 2; }
  Disabled();
  auto device = Require(CreateDevice({.Validation = true, .GpuTiming = true}));
  CHECK(device->Limits().GpuTimestamps, "this hardware proves actual GPU timestamp support");
  const auto oldest = PendingAndReady(*device, argv[1]);
  Ownership(*device, argv[1], oldest);
  Bounded(*device, argv + 1, oldest);
  auto inFlight = Prepare(*device, argv[1], kLongSteps);
  static_cast<void>(Execute(*device, inFlight, 1));
  device.reset();
  return Report();
}
