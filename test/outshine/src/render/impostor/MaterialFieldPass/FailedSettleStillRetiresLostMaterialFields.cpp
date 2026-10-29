#include "SceneRenderer.h"
#include "MaterialFieldPass.h"
#include "test/harness/render/NativeDeviceProxy.h"
#include "test/harness/render/NativeGpuCheck.h"
#include "test/harness/render/NativeMaterialField.h"
#include "Check.h"
#include <SDL3/SDL.h>
#include <chrono>
#include <expected>
#include <memory>

namespace {
using namespace outshine;
using namespace outshine::Render;
using namespace outshine::Render::Native;
using namespace outshine::Test;
using namespace outshine::Test::Gpu;

struct LossState {
  bool Lost = false;
  bool WaitFailed = false;
  unsigned Polls = 0;
};

class LossDevice final : public NativeDeviceProxy {
public:
  LossDevice(Device &device, LossState &state) : NativeDeviceProxy(device), State_(state) {}

  Result<CompletionStatus> Poll(Completion completion) override {
    if (!State_.Lost || !State_.WaitFailed) { return Backing().Poll(completion); }
    ++State_.Polls;
    return std::unexpected(Error{.Kind = ErrorKind::DeviceLost, .Message = "injected device loss"});
  }

  Result<CompletionStatus> Wait(Completion completion, std::chrono::nanoseconds budget) override {
    if (!State_.Lost) { return Backing().Wait(completion, budget); }
    State_.WaitFailed = true;
    return std::unexpected(Error{.Kind = ErrorKind::DeviceLost, .Message = "injected device loss"});
  }

private:
  LossState &State_;
};

struct Context {
  Device &DeviceRef;
  LossState &Loss;
};

void Exercise(bool stored) {
  auto device = Require(CreateDevice({.Validation = true}));
  LossState loss;
  Context context{.DeviceRef = *device, .Loss = loss};
  SceneRenderer renderer({.Context = &context, .Create = [](void *pointer, DeviceOptions) {
                            const auto &state = *static_cast<const Context *>(pointer);
                            return Result<std::unique_ptr<Device>>(
                                std::make_unique<LossDevice>(state.DeviceRef, state.Loss));
                          }});
  Require(renderer.ConfigureMaterialFields());
  const auto input =
      stored ? PreparedStoredMaterialField() : PreparedMaterialField({1, 0, 0, 1}, true);
  const auto source = Require(renderer.PrepareMaterialField(input));
  MaterialFieldView view;
  view.Camera.Kind = CameraKind::Orthographic;
  view.Camera.EyeM = {{0, 0, 2}};
  view.Camera.XMagM = view.Camera.YMagM = 1;
  view.Camera.ZNearM = .1;
  view.Camera.ZFarM = 10;
  view.Target = {.WidthPx = 16, .HeightPx = 16};
  if (stored) {
    view.Camera.EyeM = {{0, 0, 3}};
    view.Camera.ZNearM = 1;
    view.Camera.ZFarM = 5;
  }
  const auto images = Require(renderer.RelightMaterialField(*source, view));
  Require(device->Wait(images.Filtered->Produced, std::chrono::seconds(5)));
  loss.Lost = true;
  renderer.WaitForGpu();
  CHECK(loss.Polls == 1,
        "failed explicit settle still retires isolated work through its device-loss contract");
  loss.Lost = false;
  renderer.WaitForGpu();
}
}

int main() {
  CHECK(SDL_Init(SDL_INIT_VIDEO), "video initializes for native field retirement");
  Exercise(false);
  Exercise(true);
  SDL_Quit();
  return outshine::Test::Report();
}
