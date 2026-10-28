#include "test/harness/render/NativeDeviceProxy.h"
#include "test/harness/render/NativeGpuCheck.h"
#include "MaterialFieldPass.h"
#include "test/harness/render/NativeCommandsProxy.h"
#include "test/harness/render/NativeMaterialField.h"
#include "test/harness/render/NativeUploadFaultDevice.h"
#include "test/harness/render/NativeOffline.h"
#include "Check.h"
#include <SDL3/SDL.h>
#include <atomic>
#include <chrono>
#include <memory>
#include <utility>

namespace {
using namespace outshine;
using namespace outshine::Render;
using namespace outshine::Render::Native;
using namespace outshine::Test;
using namespace outshine::Test::Gpu;

class CancelCopy final : public NativeCommandsProxy {
public:
  CancelCopy(std::unique_ptr<Commands> commands, std::atomic_bool &stop)
      : NativeCommandsProxy(std::move(commands)), Stop_(stop) {}

  using NativeCommandsProxy::Copy;

  Result<void> Copy(const Buffer &from, Buffer &into, BufferCopy range) override {
    Stop_.store(true, std::memory_order_relaxed);
    return NativeCommandsProxy::Copy(from, into, range);
  }

private:
  std::atomic_bool &Stop_;
};

class CancelDevice final : public NativeDeviceProxy {
public:
  explicit CancelDevice(Device &device) : NativeDeviceProxy(device) {}

  std::atomic_bool Stop{false};
  bool Cancel = false;
  unsigned Submitted = 0;

  Result<std::unique_ptr<Commands>> Begin() override {
    auto commands = Backing().Begin();
    if (!commands || !Cancel) { return commands; }
    return std::unique_ptr<Commands>(new CancelCopy(std::move(*commands), Stop));
  }

  Result<Completion> Submit(std::unique_ptr<Commands> commands) override {
    ++Submitted;
    if (auto *wrapped = dynamic_cast<CancelCopy *>(commands.get())) { commands = wrapped->Take(); }
    return Backing().Submit(std::move(commands));
  }
};

void Exercise() {
  auto actual = Require(CreateDevice({.Validation = true}));
  UploadFailure failures;
  NativeUploadFaultDevice device(std::move(actual), failures);
  auto input = PreparedMaterialField({.4f, .5f, .6f, 1});
  MaterialFieldPass pass(device);
  Require(pass.Configure());
  failures.Point = UploadFailurePoint::DeviceBuffer;
  failures.Kind = ErrorKind::OutOfMemory;
  failures.Armed = true;
  const auto failed = pass.Prepare(input);
  CHECK(!failed && failed.error().Kind == ErrorKind::OutOfMemory && failures.Rejected == 1,
        "a staged buffer allocation failure retains its native kind through legacy bridges");
  CHECK(failures.AcceptedSubmissions == 0, "partial failed sources submit no work");
  const std::atomic_bool stop{true};
  const auto stopped = pass.Prepare(input, &stop);
  CHECK(!stopped && stopped.error().Kind == ErrorKind::Cancelled &&
            failures.AcceptedSubmissions == 0,
        "an already cancelled input creates no producer or misleading ready source");
  auto source = Require(RetryOffline([&] { return pass.Prepare(input); }, std::chrono::seconds(5)));
  const auto produced = pass.LastSubmission();
  source.reset();
  CHECK(produced && failures.AcceptedSubmissions == 1,
        "a new source can succeed after the isolated failed attempt is discarded");
  if (produced) { Require(device.Wait(*produced, std::chrono::seconds(5))); }
  Require(pass.Collect());
  CancelDevice cancelling(device);
  MaterialFieldPass cancelled(cancelling);
  Require(cancelled.Configure());
  cancelling.Cancel = true;
  const auto aborted = cancelled.Prepare(input, &cancelling.Stop);
  CHECK(!aborted && aborted.error().Kind == ErrorKind::Cancelled &&
            cancelling.Stop.load(std::memory_order_relaxed) && cancelling.Submitted == 0 &&
            !cancelled.LastSubmission(),
        "cancellation during copy recording discards commands before source publication");
  Require(cancelled.Collect());
}
}

int main() {
  CHECK(SDL_Init(SDL_INIT_VIDEO), "video initializes for native resources");
  Exercise();
  SDL_Quit();
  return Report();
}
