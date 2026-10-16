#include "NativeGpuCheck.h"
#include "Check.h"
#include <array>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <print>
#include <thread>
#include <utility>

namespace {
using namespace outshine::Render::Native;
using namespace outshine::Test;
using namespace outshine::Test::Gpu;
constexpr uint32_t kSeed = 7;
constexpr uint32_t kSteps = 20000000;
constexpr uint32_t kMultiplier = 1664525;
constexpr uint32_t kIncrement = 1013904223;
constexpr uint32_t kNewerSubmissions = 128;
constexpr auto kObservationDelay = std::chrono::milliseconds(50);

void Complete(Device &device, Completion completion) {
  CHECK(Require(device.Wait(completion, std::chrono::seconds(5))) == CompletionStatus::Ready,
        "bounded wait completes the actual submission");
}

Completion Empty(Device &device) {
  const auto completion = Require(device.Submit(Require(device.Begin())));
  Complete(device, completion);
  return completion;
}

void Disabled() {
  auto device = Require(CreateDevice({.Validation = true}));
  const auto timing = device->SubmissionGpuTime(Empty(*device));
  CHECK(!timing && timing.error().Kind == ErrorKind::Unsupported,
        "default devices explicitly leave timing disabled");
}

void Ownership(Device &device, Completion completion) {
  auto foreign = completion;
  ++foreign.Device;
  const std::array invalid{
      Completion{},
      foreign,
      Completion{.Device = completion.Device, .Serial = completion.Serial + 1}};
  for (const auto token : invalid) {
    const auto timing = device.SubmissionGpuTime(token);
    CHECK(!timing && timing.error().Kind == ErrorKind::Invalid,
          "missing, foreign and unsubmitted tokens have no GPU time");
  }
  bool rejected = false;
  std::thread other([&] {
    const auto timing = device.SubmissionGpuTime(completion);
    rejected = !timing && timing.error().Kind == ErrorKind::Invalid;
  });
  other.join();
  CHECK(rejected, "timing queries preserve device thread ownership");
}

void Readback(Device &device, Buffer &buffer) {
  const auto bytes = Require(device.MapRead(buffer, {.Bytes = sizeof(uint32_t)}));
  uint32_t actual = 0;
  std::memcpy(&actual, bytes.data(), sizeof(actual));
  Require(device.Unmap(buffer));
  uint32_t expected = kSeed;
  for (uint32_t at = 0; at < kSteps; ++at) {
    expected = (expected ^ (expected >> 13u)) * kMultiplier + kIncrement;
  }
  CHECK(actual == expected, "timed copy/compute/readback preserves independent CPU arithmetic");
}

Completion Measured(Device &device, const char *shader) {
  const auto pipeline = Require(device.CreateComputePipeline(Spirv(shader)));
  auto readback = Require(device.CreateBuffer({.Bytes = sizeof(uint32_t),
                                               .Use = BufferUse::CopyDestination,
                                               .Placement = Memory::Readback}));
  auto commands = Require(device.Begin());
  const std::array values{kSeed};
  const auto buffer = Upload(device, *commands, values);
  const std::array bindings{Storage(0, 0, *buffer)};
  Require(commands->Dispatch(*pipeline, bindings, {.X = 1, .Y = 1, .Z = 1}));
  Require(commands->Copy(*buffer, *readback, {.Bytes = sizeof(uint32_t)}));
  const auto completion = Require(device.Submit(std::move(commands)));
  const auto queryStart = std::chrono::steady_clock::now();
  const auto first = Require(device.SubmissionGpuTime(completion));
  const auto queryElapsed = std::chrono::steady_clock::now() - queryStart;
  if (first.Status == CompletionStatus::Pending) {
    CHECK(std::isnan(first.ElapsedNanoseconds), "Pending does not report an invented duration");
  } else {
    CHECK(std::isfinite(first.ElapsedNanoseconds) && first.ElapsedNanoseconds >= 0,
          "already completed work may return an actual finite time immediately");
  }
  Complete(device, completion);
  const auto timing = Require(device.SubmissionGpuTime(completion));
  CHECK(timing.Status == CompletionStatus::Ready && std::isfinite(timing.ElapsedNanoseconds) &&
            timing.ElapsedNanoseconds > 0,
        "real dependent GPU work has a positive completed GPU span");
  Readback(device, *readback);
  std::this_thread::sleep_for(kObservationDelay);
  auto again = Require(device.SubmissionGpuTime(completion));
  CHECK(again.ElapsedNanoseconds == timing.ElapsedNanoseconds,
        "delayed CPU observation does not grow GPU execution time");
  again.ElapsedNanoseconds = 0;
  CHECK(Require(device.SubmissionGpuTime(completion)).ElapsedNanoseconds ==
            timing.ElapsedNanoseconds,
        "caller mutation does not modify retained timing metadata");
  std::println("submission_gpu_ns={} query_cpu_ns={} observed_pending={}",
               timing.ElapsedNanoseconds,
               std::chrono::duration_cast<std::chrono::nanoseconds>(queryElapsed).count(),
               first.Status == CompletionStatus::Pending);
  return completion;
}

void Bounded(Device &device, Completion oldest) {
  Completion latest;
  for (uint32_t at = 0; at < kNewerSubmissions; ++at) { latest = Empty(device); }
  const auto expired = device.SubmissionGpuTime(oldest);
  CHECK(!expired && expired.error().Kind == ErrorKind::Invalid,
        "bounded metadata expires old results explicitly while resources retire normally");
  const auto current = Require(device.SubmissionGpuTime(latest));
  CHECK(current.Status == CompletionStatus::Ready && std::isfinite(current.ElapsedNanoseconds) &&
            current.ElapsedNanoseconds >= 0,
        "empty submissions also report actual bounded GPU execution time");
  CHECK(Require(device.Poll(oldest)) == CompletionStatus::Ready,
        "timing expiry preserves the completion contract");
}

}

int main(int argc, const char *const *argv) {
  if (argc != 2) { return 2; }
  Disabled();
  auto device = Require(CreateDevice({.Validation = true, .GpuTiming = true}));
  const auto completion = Measured(*device, argv[1]);
  Ownership(*device, completion);
  Bounded(*device, completion);
  return Report();
}
