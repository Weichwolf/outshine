#ifndef OUTSHINE_TEST_RENDER_NATIVEDEVICEPROXY_H
#define OUTSHINE_TEST_RENDER_NATIVEDEVICEPROXY_H

#include "src/render/device/native/Device.h"
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <span>
#include <string_view>
#include <utility>

namespace outshine::Test::Gpu {

class NativeDeviceProxy : public Render::Native::Device {
public:
  explicit NativeDeviceProxy(Render::Native::Device &device) : Backing_(device) {}

  [[nodiscard]] const Render::Native::Capabilities &Limits() const noexcept override {
    return Backing_.Limits();
  }

  [[nodiscard]] bool Supports(Render::Native::Format format,
                              Render::Native::UseMask<Render::Native::TextureUse> use,
                              bool filtered) const override {
    return Backing_.Supports(format, use, filtered);
  }

  [[nodiscard]] Render::Native::Result<std::unique_ptr<Render::Native::Buffer>>
  CreateBuffer(Render::Native::BufferDescription description,
               std::string_view label = {}) override {
    return Backing_.CreateBuffer(description, label);
  }

  [[nodiscard]] Render::Native::Result<std::unique_ptr<Render::Native::Texture>>
  CreateTexture(Render::Native::TextureDescription description,
                std::string_view label = {}) override {
    return Backing_.CreateTexture(description, label);
  }

  [[nodiscard]] Render::Native::Result<std::unique_ptr<Render::Native::Sampler>>
  CreateSampler(Render::Native::SamplerDescription description,
                std::string_view label = {}) override {
    return Backing_.CreateSampler(description, label);
  }

  [[nodiscard]] Render::Native::Result<std::unique_ptr<Render::Native::ComputePipeline>>
  CreateComputePipeline(std::span<const uint32_t> spirv,
                        Render::Native::ComputeOptions options = {}) override {
    return Backing_.CreateComputePipeline(spirv, options);
  }

  [[nodiscard]] Render::Native::Result<std::unique_ptr<Render::Native::GraphicsPipeline>>
  CreateGraphicsPipeline(const Render::Native::GraphicsDescription &description) override {
    return Backing_.CreateGraphicsPipeline(description);
  }

  [[nodiscard]] Render::Native::Result<std::span<std::byte>>
  MapWrite(Render::Native::Buffer &buffer, Render::Native::ByteRange range) override {
    return Backing_.MapWrite(buffer, range);
  }

  [[nodiscard]] Render::Native::Result<std::span<const std::byte>>
  MapRead(Render::Native::Buffer &buffer, Render::Native::ByteRange range) override {
    return Backing_.MapRead(buffer, range);
  }

  [[nodiscard]] Render::Native::Result<void> Unmap(Render::Native::Buffer &buffer) override {
    return Backing_.Unmap(buffer);
  }

  [[nodiscard]] Render::Native::Result<std::unique_ptr<Render::Native::Commands>> Begin() override {
    return Backing_.Begin();
  }

  [[nodiscard]] Render::Native::Result<void> SetPresentTarget(SDL_Window *window) override {
    return Backing_.SetPresentTarget(window);
  }

  [[nodiscard]] Render::Native::Result<std::unique_ptr<Render::Native::PresentFrame>>
  AcquirePresentFrame() override {
    return Backing_.AcquirePresentFrame();
  }

  [[nodiscard]] Render::Native::Result<Render::Native::Completion>
  Submit(std::unique_ptr<Render::Native::Commands> commands) override {
    return Backing_.Submit(std::move(commands));
  }

  [[nodiscard]] Render::Native::Result<Render::Native::CompletionStatus>
  Poll(Render::Native::Completion completion) override {
    return Backing_.Poll(completion);
  }

  [[nodiscard]] Render::Native::Result<Render::Native::CompletionStatus>
  Wait(Render::Native::Completion completion, std::chrono::nanoseconds budget) override {
    return Backing_.Wait(completion, budget);
  }

  [[nodiscard]] Render::Native::Result<Render::Native::GpuTimingResults>
  GpuTimings(Render::Native::Completion completion) override {
    return Backing_.GpuTimings(completion);
  }

  [[nodiscard]] Render::Native::Result<Render::Native::SubmissionGpuTiming>
  SubmissionGpuTime(Render::Native::Completion completion) override {
    return Backing_.SubmissionGpuTime(completion);
  }

protected:
  [[nodiscard]] Render::Native::Device &Backing() const noexcept { return Backing_; }

private:
  Render::Native::Device &Backing_;
};

}
#endif
