#ifndef OUTSHINE_RENDER_DEVICE_GPUSUBMISSION_H
#define OUTSHINE_RENDER_DEVICE_GPUSUBMISSION_H

#include "native/Device.h"
#include <chrono>
#include <memory>
#include <utility>

namespace outshine::Render {
struct GpuSubmission {
  void *Context = nullptr;
  Native::Result<std::unique_ptr<Native::Device>> (*Create)(void *, Native::DeviceOptions) =
      [](void *, Native::DeviceOptions options) { return Native::CreateDevice(options); };
  Native::Result<std::unique_ptr<Native::Commands>> (*Acquire)(void *, Native::Device *) =
      [](void *, Native::Device *device) { return device->Begin(); };
  Native::Result<Native::Completion> (*Submit)(void *,
                                               Native::Device *,
                                               std::unique_ptr<Native::Commands>) =
      [](void *, Native::Device *device, std::unique_ptr<Native::Commands> commands) {
        return device->Submit(std::move(commands));
      };
  Native::Result<Native::CompletionStatus> (*QueryFence)(void *,
                                                         Native::Device *,
                                                         Native::Completion) =
      [](void *, Native::Device *device, Native::Completion completion) {
        return device->Poll(completion);
      };
  Native::Result<Native::CompletionStatus> (*WaitFence)(void *,
                                                        Native::Device *,
                                                        Native::Completion,
                                                        std::chrono::nanoseconds) =
      [](void *,
         Native::Device *device,
         Native::Completion completion,
         std::chrono::nanoseconds budget) { return device->Wait(completion, budget); };
};
}
#endif
