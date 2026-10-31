#ifndef OUTSHINE_TEST_RENDER_NATIVEDRAWCONTROL_H
#define OUTSHINE_TEST_RENDER_NATIVEDRAWCONTROL_H

#include "NativeCommandsProxy.h"
#include "NativeDeviceProxy.h"
#include <cstddef>
#include <cstdint>
#include <memory>
#include <utility>

namespace outshine::Test::Gpu {
enum class NativeDrawMode { Execute, Omit };

struct NativeDrawControl {
  NativeDrawMode Mode = NativeDrawMode::Execute;
  size_t DeviceCreations = 0;
  uint64_t Forwarded = 0, Omitted = 0;
};

class NativeControlledCommands final : public NativeCommandsProxy {
public:
  NativeControlledCommands(std::unique_ptr<Render::Native::Commands> commands,
                           NativeDrawControl &control)
      : NativeCommandsProxy(std::move(commands)), Control_(control) {}

  [[nodiscard]] Render::Native::Result<void>
  Draw(const Render::Native::DrawDescription &description) override {
    if (Omit()) { return {}; }
    return NativeCommandsProxy::Draw(description);
  }

  [[nodiscard]] Render::Native::Result<void>
  DrawIndexed(const Render::Native::IndexedDrawDescription &description) override {
    if (Omit()) { return {}; }
    return NativeCommandsProxy::DrawIndexed(description);
  }

  [[nodiscard]] Render::Native::Result<void>
  DrawIndexedIndirect(const Render::Native::IndirectDrawDescription &description) override {
    if (Omit()) { return {}; }
    return NativeCommandsProxy::DrawIndexedIndirect(description);
  }

private:
  [[nodiscard]] bool Omit() noexcept {
    if (Control_.Mode == NativeDrawMode::Omit) {
      ++Control_.Omitted;
      return true;
    }
    ++Control_.Forwarded;
    return false;
  }

  NativeDrawControl &Control_;
};

class NativeControlledDevice final : public NativeDeviceProxy {
public:
  NativeControlledDevice(std::unique_ptr<Render::Native::Device> device, NativeDrawControl &control)
      : NativeDeviceProxy(*device), Device_(std::move(device)), Control_(control) {}

  [[nodiscard]] Render::Native::Result<std::unique_ptr<Render::Native::Commands>> Begin() override {
    auto commands = Backing().Begin();
    if (!commands) { return commands; }
    return std::unique_ptr<Render::Native::Commands>(
        new NativeControlledCommands(std::move(*commands), Control_));
  }

  [[nodiscard]] Render::Native::Result<Render::Native::Completion>
  Submit(std::unique_ptr<Render::Native::Commands> commands) override {
    if (auto *wrapped = dynamic_cast<NativeControlledCommands *>(commands.get())) {
      commands = wrapped->Take();
    }
    return Backing().Submit(std::move(commands));
  }

private:
  std::unique_ptr<Render::Native::Device> Device_;
  NativeDrawControl &Control_;
};
}
#endif
