#ifndef OUTSHINE_TEST_RENDER_NATIVETARGETFAULTDEVICE_H
#define OUTSHINE_TEST_RENDER_NATIVETARGETFAULTDEVICE_H

#include "NativeDeviceProxy.h"
#include "NativeTargetTextures.h"
#include <chrono>
#include <expected>
#include <memory>
#include <span>
#include <string_view>
#include <utility>

namespace outshine::Test::Gpu {
enum class TargetFailure { None, Present, Wait, Texture, Sampler, Buffer, Pipeline };

struct TargetFault {
  TargetFailure Point = TargetFailure::None;
  unsigned FailAt = 1;
  unsigned Attempt = 0;
  unsigned Rejected = 0;
  unsigned CreatedTextures = 0;
  unsigned ReleasedTextures = 0;
  unsigned CreatedSamplers = 0;
  unsigned CreatedBuffers = 0;
  unsigned CreatedPipelines = 0;
  SDL_Window *Window = nullptr;

  void Arm(TargetFailure point, unsigned at = 1) {
    Point = point;
    FailAt = at;
    Attempt = 0;
  }

  bool Reject(TargetFailure point) {
    if (Point != point || ++Attempt != FailAt) { return false; }
    ++Rejected;
    Point = TargetFailure::None;
    return true;
  }

  [[nodiscard]] static TargetNative::Error Error() {
    return {.Kind = TargetNative::ErrorKind::Driver, .Message = "injected native target failure"};
  }
};

class NativeTargetFaultDevice final : public NativeDeviceProxy {
public:
  NativeTargetFaultDevice(std::unique_ptr<TargetNative::Device> device, TargetFault &fault)
      : NativeDeviceProxy(*device), Device_(std::move(device)), Fault_(fault) {}

  [[nodiscard]] static TargetNative::Result<std::unique_ptr<TargetNative::Device>>
  Create(void *context, TargetNative::DeviceOptions options) {
    auto device = TargetNative::CreateDevice(options);
    if (!device) { return std::unexpected(device.error()); }
    return std::make_unique<NativeTargetFaultDevice>(std::move(*device),
                                                     *static_cast<TargetFault *>(context));
  }

  [[nodiscard]] TargetNative::Result<std::unique_ptr<TargetNative::Texture>>
  CreateTexture(TargetNative::TextureDescription description, std::string_view label) override {
    if (Fault_.Reject(TargetFailure::Texture)) { return std::unexpected(TargetFault::Error()); }
    auto texture = Backing().CreateTexture(description, label);
    if (!texture) { return std::unexpected(texture.error()); }
    ++Fault_.CreatedTextures;
    return std::make_unique<TargetTexture>(std::move(*texture), Fault_.ReleasedTextures);
  }

  [[nodiscard]] TargetNative::Result<std::unique_ptr<TargetNative::Buffer>>
  CreateBuffer(TargetNative::BufferDescription description, std::string_view label) override {
    if (Fault_.Reject(TargetFailure::Buffer)) { return std::unexpected(TargetFault::Error()); }
    auto buffer = Backing().CreateBuffer(description, label);
    Fault_.CreatedBuffers += buffer ? 1u : 0u;
    return buffer;
  }

  [[nodiscard]] TargetNative::Result<std::unique_ptr<TargetNative::Sampler>>
  CreateSampler(TargetNative::SamplerDescription description, std::string_view label) override {
    if (Fault_.Reject(TargetFailure::Sampler)) { return std::unexpected(TargetFault::Error()); }
    auto sampler = Backing().CreateSampler(description, label);
    Fault_.CreatedSamplers += sampler ? 1u : 0u;
    return sampler;
  }

  [[nodiscard]] TargetNative::Result<std::unique_ptr<TargetNative::GraphicsPipeline>>
  CreateGraphicsPipeline(const TargetNative::GraphicsDescription &description) override {
    if (Fault_.Reject(TargetFailure::Pipeline)) { return std::unexpected(TargetFault::Error()); }
    auto pipeline = Backing().CreateGraphicsPipeline(description);
    Fault_.CreatedPipelines += pipeline ? 1u : 0u;
    return pipeline;
  }

  [[nodiscard]] TargetNative::Result<void> SetPresentTarget(SDL_Window *window) override {
    if (Fault_.Reject(TargetFailure::Present)) { return std::unexpected(TargetFault::Error()); }
    auto result = Backing().SetPresentTarget(window);
    if (result) { Fault_.Window = window; }
    return result;
  }

  [[nodiscard]] TargetNative::Result<std::unique_ptr<TargetNative::Commands>> Begin() override {
    auto commands = Backing().Begin();
    if (!commands) { return std::unexpected(commands.error()); }
    return std::make_unique<TargetTextureCommands>(std::move(*commands));
  }

  [[nodiscard]] TargetNative::Result<TargetNative::Completion>
  Submit(std::unique_ptr<TargetNative::Commands> commands) override {
    if (auto *wrapped = dynamic_cast<TargetTextureCommands *>(commands.get())) {
      return Backing().Submit(wrapped->Take());
    }
    return Backing().Submit(std::move(commands));
  }

  [[nodiscard]] TargetNative::Result<TargetNative::CompletionStatus>
  Wait(TargetNative::Completion completion, std::chrono::nanoseconds budget) override {
    if (Fault_.Reject(TargetFailure::Wait)) { return std::unexpected(TargetFault::Error()); }
    return Backing().Wait(completion, budget);
  }

private:
  std::unique_ptr<TargetNative::Device> Device_;
  TargetFault &Fault_;
};
}
#endif
