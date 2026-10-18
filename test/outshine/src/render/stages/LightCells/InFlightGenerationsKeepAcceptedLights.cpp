#include "Check.h"
#include "SubjectLights.h"
#include "UploadQueue.h"
#include "test/harness/render/NativeDeviceProxy.h"
#include "test/harness/render/NativeGpuCheck.h"
#include <SDL3/SDL.h>
#include <array>
#include <chrono>
#include <cstddef>
#include <expected>
#include <limits>
#include <memory>
#include <optional>
#include <string_view>
#include <utility>

namespace {
using namespace outshine;
using namespace outshine::Render;
using namespace outshine::Render::Native;
using namespace outshine::Test;
using outshine::Test::Gpu::Require;

class Observed final : public outshine::Test::Gpu::NativeDeviceProxy {
public:
  using NativeDeviceProxy::NativeDeviceProxy;
  unsigned Buffers = 0, Begins = 0, Waits = 0;
  bool BlockPoll = false, BlockBegin = false;
  std::optional<Completion> Last;

  Result<std::unique_ptr<Buffer>> CreateBuffer(BufferDescription description,
                                               std::string_view label) override {
    ++Buffers;
    return Backing().CreateBuffer(description, label);
  }

  Result<std::unique_ptr<Commands>> Begin() override {
    ++Begins;
    if (std::exchange(BlockBegin, false)) {
      return std::unexpected(Error{.Kind = ErrorKind::Busy, .Message = "injected admission"});
    }
    return Backing().Begin();
  }

  [[nodiscard]] Completion Accepted() const {
    CHECK(Last.has_value(), "a GPU completion was accepted");
    if (!Last) { outshine::Test::Gpu::Failed(); }
    return *Last;
  }

  Result<Completion> Submit(std::unique_ptr<Commands> commands) override {
    auto accepted = Backing().Submit(std::move(commands));
    if (accepted) { Last = *accepted; }
    return accepted;
  }

  Result<CompletionStatus> Poll(Completion completion) override {
    return BlockPoll ? Result<CompletionStatus>(CompletionStatus::Pending)
                     : Backing().Poll(completion);
  }

  Result<CompletionStatus> Wait(Completion completion, std::chrono::nanoseconds budget) override {
    ++Waits;
    return Backing().Wait(completion, budget);
  }
};

constexpr auto kWait = std::chrono::seconds(5);

void Updates(Observed &device, Device &actual) {
  std::chrono::nanoseconds waitBudget = kWait;
  UploadQueue uploads(device, waitBudget);
  SubjectLights lights;
  std::array<SubjectLight, kMaxSubjectLights + 1> source{};
  for (auto &entry : source) {
    entry.Light.Kind = LightKind::Point;
    entry.Light.RangeM = 16;
    entry.PositionEcefM = {{1, 2, 3}};
  }
  Require(lights.Set(source));
  Require(lights.Attach(device, &uploads));
  CHECK(lights.Spatial() && lights.Emitters() && lights.Index(),
        "all seventeen sources publish one accepted spatial generation");
  CHECK(Require(actual.Wait(device.Accepted(), kWait)) == CompletionStatus::Ready,
        "the initial actual upload reaches completion");
  const auto *original = lights.Emitters();
  const unsigned begins = device.Begins;
  const unsigned buffers = device.Buffers;
  lights.Record();
  Require(lights.Set(source));
  CHECK(device.Begins == begins && device.Buffers == buffers && lights.Emitters() == original,
        "unchanged sources reuse their generation even with a recorded reader");
  source.back().Light.Intensity = 2;
  const auto recorded = lights.Set(source);
  CHECK(!recorded && recorded.error().Kind == ErrorKind::Busy &&
            lights.Sources().back().Light.Intensity == 1,
        "a source edit cannot replace an unsubmitted reader's generation");
  lights.Aborted();
  Require(lights.Set(source));
  CHECK(lights.Emitters() != original && lights.Sources().back().Light.Intensity == 2,
        "aborting a recording releases publication without waiting");
  source.back().Light.Intensity = 3;
  device.BlockPoll = true;
  Require(lights.Set(source));
  const auto *accepted = lights.Emitters();
  source.back().Light.Intensity = 4;
  const auto busy = lights.Set(source);
  CHECK(!busy && busy.error().Kind == ErrorKind::Busy && lights.Emitters() == accepted &&
            lights.Sources().back().Light.Intensity == 3 && device.Waits == 0,
        "three occupied slots retain accepted lights and never block on a fourth update");
  Observed replacement(actual);
  const auto occupiedDevice = lights.Attach(replacement, nullptr);
  CHECK(!occupiedDevice && occupiedDevice.error().Kind == ErrorKind::Busy &&
            lights.Emitters() == accepted && device.Waits == 0,
        "device replacement cannot destroy a generation with outstanding readers");
  device.BlockPoll = false;
  CHECK(Require(actual.Wait(device.Accepted(), kWait)) == CompletionStatus::Ready,
        "actual prior uploads are retired before the retry");
  device.BlockBegin = true;
  const auto refused = lights.Set(source);
  CHECK(!refused && refused.error().Kind == ErrorKind::Busy && lights.Emitters() == accepted &&
            lights.Sources().back().Light.Intensity == 3 && device.Waits == 0,
        "upload admission failure preserves the complete accepted generation without waiting");
  Require(lights.Set(source));
  CHECK(lights.Sources().back().Light.Intensity == 4,
        "a retired slot accepts the same edit on a later retry");
  lights.Record();
  const auto reader = Require(device.Submit(Require(device.Begin())));
  lights.Submitted(reader);
  source.back().Light.Intensity = 5;
  Require(lights.Set(source));
  source.back().PositionEcefM[0] = std::numeric_limits<double>::quiet_NaN();
  const auto invalid = lights.Set(source);
  CHECK(!invalid && lights.Sources().back().Light.Intensity == 5,
        "invalid input retains the last complete source and GPU generation");
  CHECK(Require(actual.Wait(device.Accepted(), kWait)) == CompletionStatus::Ready,
        "all accepted generations retire before fixture destruction");
}
}

int main() {
  CHECK(SDL_Init(SDL_INIT_VIDEO), "the native GPU fixture initializes its platform lifetime");
  {
    auto actual = Require(CreateDevice({.Validation = true}));
    Observed device(*actual);
    Updates(device, *actual);
  }
  SDL_Quit();
  return Report();
}
