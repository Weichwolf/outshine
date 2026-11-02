#include "FieldPageFeedbackPass.h"
#include "FieldPageFeedback.h"
#include "ComputeShaders.h"
#include "PipelineFactory.h"
#include <array>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <limits>
#include <optional>
#include <span>
#include <utility>
#include <variant>

namespace outshine::Render {
namespace {
struct alignas(16) FeedbackSampling {
  uint32_t Width, Height, Stride, Phase;
};

static_assert(sizeof(FeedbackSampling) == 16 && alignof(FeedbackSampling) == 16 &&
              offsetof(FeedbackSampling, Width) == 0 && offsetof(FeedbackSampling, Height) == 4 &&
              offsetof(FeedbackSampling, Stride) == 8 && offsetof(FeedbackSampling, Phase) == 12);
constexpr auto kKernel = FindComputeShader(ComputeShaderId::FieldPageFeedback)->Shape;
}

Native::Result<void> FieldPageFeedbackPass::Configure(Native::Device &device,
                                                      const Native::Texture &source,
                                                      FieldPageFeedbackOptions options) {
  const auto stride = options.Stride;
  const auto storageBytesMost = options.StorageBytesMost;
  const auto &shape = source.Description();
  const bool sampled =
      (static_cast<uint32_t>(shape.Use) & static_cast<uint32_t>(Native::TextureUse::Sampled)) != 0;
  if (shape.Texels != Native::Format::Rgba32Float || !sampled ||
      shape.Type != Native::TextureType::Image2D || shape.Layers != 1 || stride == 0 ||
      stride > device.Limits().MaxTextureExtent ||
      stride > std::numeric_limits<uint32_t>::max() / stride) {
    return std::unexpected(Native::Error{.Kind = Native::ErrorKind::Invalid,
                                         .Message = "invalid field page feedback image or stride"});
  }
  const uint64_t across = (uint64_t{shape.Width} + stride - 1) / stride;
  const uint64_t down = (uint64_t{shape.Height} + stride - 1) / stride;
  const uint64_t count = across * down;
  if (count > std::numeric_limits<uint64_t>::max() / sizeof(FieldPageFeedbackPixel)) {
    return std::unexpected(Native::Error{.Kind = Native::ErrorKind::Invalid,
                                         .Message = "field page feedback sample count overflow"});
  }
  const uint64_t bytes = count * sizeof(FieldPageFeedbackPixel);
  if (count == 0 || bytes > storageBytesMost / 3 || bytes > device.Limits().MaxStorageBufferBytes ||
      count > std::numeric_limits<size_t>::max() ||
      (across + kKernel.GroupX - 1) / kKernel.GroupX > device.Limits().MaxWorkgroupCount[0] ||
      (down + kKernel.GroupY - 1) / kKernel.GroupY > device.Limits().MaxWorkgroupCount[1]) {
    return std::unexpected(
        Native::Error{.Kind = Native::ErrorKind::Invalid,
                      .Message = "field page feedback exceeds admitted storage"});
  }
  if (Output_) {
    if (Device_ != &device || Source_ != &source || Stride_ != stride) {
      return std::unexpected(
          Native::Error{.Kind = Native::ErrorKind::Invalid,
                        .Message = "field page feedback already owns another source"});
    }
    return {};
  }
  auto pipeline = CreateComputePipeline(device, ComputeShaderId::FieldPageFeedback);
  if (!pipeline) { return std::unexpected(pipeline.error()); }
  auto sampler = device.CreateSampler({.Min = Native::Filter::Nearest,
                                       .Mag = Native::Filter::Nearest,
                                       .Mip = Native::Filter::Nearest,
                                       .MaxLod = 0});
  if (!sampler) { return std::unexpected(sampler.error()); }
  auto output = device.CreateBuffer(
      {.Bytes = bytes, .Use = Native::BufferUse::Storage | Native::BufferUse::CopySource});
  if (!output) { return std::unexpected(output.error()); }
  Device_ = &device;
  Source_ = &source;
  Stride_ = stride;
  Samples_ = static_cast<size_t>(count);
  Pipeline_ = std::move(*pipeline);
  Sampling_ = std::move(*sampler);
  Output_ = std::move(*output);
  return {};
}

Native::Result<bool> FieldPageFeedbackPass::Encode(Native::Commands &commands, uint32_t phase) {
  if (!Output_ || phase >= Stride_ * Stride_) {
    return std::unexpected(
        Native::Error{.Kind = Native::ErrorKind::Invalid,
                      .Message = "field page feedback has no valid sampling phase"});
  }
  if (std::holds_alternative<ReadbackTransfer>(State_)) { return false; }
  const auto &source = Source_->Description();
  const FeedbackSampling over{
      .Width = source.Width, .Height = source.Height, .Stride = Stride_, .Phase = phase};
  const std::array bindings{
      Native::Binding{.Set = 0,
                      .Slot = 0,
                      .Value = Native::SampledTexture{.Image = {.Image = Source_},
                                                      .Sampling = Sampling_.get()}},
      Native::Binding{.Set = 1,
                      .Slot = 0,
                      .Value =
                          Native::StorageBuffer{.Data = Output_.get(),
                                                .Range = {.Bytes = Output_->Description().Bytes}}},
      Native::Binding{.Set = 2,
                      .Slot = 0,
                      .Value = Native::Uniform{.Bytes = std::as_bytes(std::span(&over, 1))}}};
  const auto recorded = commands.Dispatch(
      *Pipeline_,
      bindings,
      {.X = ((source.Width + Stride_ - 1) / Stride_ + kKernel.GroupX - 1) / kKernel.GroupX,
       .Y = ((source.Height + Stride_ - 1) / Stride_ + kKernel.GroupY - 1) / kKernel.GroupY,
       .Z = 1});
  if (!recorded) { return std::unexpected(recorded.error()); }
  State_ = Recorded{};
  return true;
}

Native::Result<bool> FieldPageFeedbackPass::BeginReadback() {
  if (!std::holds_alternative<Recorded>(State_)) { return false; }
  auto pending =
      ReadbackTransfer::Start(*Device_, *Output_, {.Bytes = Output_->Description().Bytes});
  if (!pending) { return std::unexpected(pending.error()); }
  State_ = std::move(*pending);
  return true;
}

Native::Result<std::optional<ReadbackTransfer>> FieldPageFeedbackPass::Take() {
  auto *pending = std::get_if<ReadbackTransfer>(&State_);
  if (pending == nullptr) { return std::optional<ReadbackTransfer>{}; }
  auto ready = pending->Poll(*Device_);
  if (!ready) { return std::unexpected(ready.error()); }
  if (!*ready) { return std::optional<ReadbackTransfer>{}; }
  auto completed = std::move(*pending);
  State_ = std::monostate{};
  return std::optional{std::move(completed)};
}

uint64_t FieldPageFeedbackPass::HeldBytes() const noexcept {
  return Output_ ? Output_->Description().Bytes *
                       (std::holds_alternative<ReadbackTransfer>(State_) ? 2u : 1u)
                 : 0;
}
}
