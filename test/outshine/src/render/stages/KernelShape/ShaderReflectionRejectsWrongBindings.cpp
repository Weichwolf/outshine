#include "Check.h"
#include "StagePipelines.h"
#include "ComputeShaders.h"
#include "KernelShape.h"
#include "Device.h"
#include "test/harness/render/NativeGpuCheck.h"
#include "src/render/device/ReadbackTransfer.h"
#include <array>
#include <bit>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <utility>

namespace {
using namespace outshine::Render;
using namespace outshine::Render::Native;
using namespace outshine::Test;
using outshine::Test::Gpu::Require;
constexpr uint32_t kSide = 8;
constexpr auto kWait = std::chrono::seconds(5);

void Rejected(Commands &commands, DrawDescription draw) {
  const auto result = commands.Draw(draw);
  CHECK(!result && result.error().Kind == ErrorKind::Invalid,
        "a binding inconsistent with actual shader reflection is refused before drawing");
}

void Graphics(Device &device) {
  auto pipeline = Require(CreatePresentPipeline(device, Format::Rgba8Unorm));
  const auto &layout = pipeline->Layout();
  CHECK(layout.Bindings.size() == 1, "the actual fullscreen shaders require exactly one resource");
  if (layout.Bindings.size() != 1) { return; }
  const auto &binding = layout.Bindings.front();
  CHECK(binding.Resource.Set == 2 && binding.Resource.Slot == 0 &&
            binding.Resource.Kind == BindingKind::SampledTexture &&
            static_cast<uint32_t>(binding.Stages) == static_cast<uint32_t>(ShaderStage::Fragment),
        "the reflected sampler belongs to the fragment stage at its authored set and slot");
  auto source =
      Require(device.CreateTexture({.Width = kSide,
                                    .Height = kSide,
                                    .Use = TextureUse::Sampled | TextureUse::ColourAttachment}));
  auto target =
      Require(device.CreateTexture({.Width = kSide,
                                    .Height = kSide,
                                    .Use = TextureUse::CopySource | TextureUse::ColourAttachment}));
  auto sampler = Require(device.CreateSampler({}));
  auto commands = Require(device.Begin());
  const std::array sourceAttachment{ColourAttachment{
      .Image = {.Image = source.get()}, .Begin = Load::Clear, .Clear = {1, 0, 0, 1}}};
  Require(commands->BeginRenderPass({.Colours = sourceAttachment, .Depth = {}}));
  Require(commands->EndRenderPass());
  const std::array targetAttachment{
      ColourAttachment{.Image = {.Image = target.get()}, .Begin = Load::Clear}};
  Require(commands->BeginRenderPass({.Colours = targetAttachment, .Depth = {}}));
  const Binding sampled{
      .Set = 2,
      .Slot = 0,
      .Value = SampledTexture{.Image = {.Image = source.get()}, .Sampling = sampler.get()}};
  DrawDescription draw{.State = {.Pipeline = pipeline.get(),
                                 .Bindings = {},
                                 .Vertices = {},
                                 .Area = {.Width = kSide, .Height = kSide},
                                 .Clip = {.Width = kSide, .Height = kSide}},
                       .Range = {.Count = 3}};
  Rejected(*commands, draw);
  for (auto wrong : {Binding{.Set = 0, .Slot = 0, .Value = sampled.Value},
                     Binding{.Set = 2, .Slot = 1, .Value = sampled.Value},
                     Binding{.Set = 2, .Slot = 0, .Value = Uniform{}}}) {
    draw.State.Bindings = std::span(&wrong, 1);
    Rejected(*commands, draw);
  }
  const std::array duplicate{sampled, sampled};
  draw.State.Bindings = duplicate;
  Rejected(*commands, draw);
  const std::array extra{sampled, Binding{.Set = 1, .Slot = 0, .Value = Uniform{}}};
  draw.State.Bindings = extra;
  Rejected(*commands, draw);
  draw.State.Bindings = std::span(&sampled, 1);
  Require(commands->Draw(draw));
  Require(commands->EndRenderPass());
  CHECK(Require(device.Wait(Require(device.Submit(std::move(commands))), kWait)) ==
            CompletionStatus::Ready,
        "the correctly bound real fullscreen draw completes");
  auto read = Require(ReadbackTransfer::Start(device, *target, {.Width = kSide, .Height = kSide}));
  const auto pixels = Require(read.Wait(device, kWait));
  CHECK(pixels.has_value(), "accepted bindings produce a readable image");
  if (!pixels) { return; }
  size_t wrongPixels = 0;
  for (uint32_t y = 0; y < kSide; ++y) {
    for (uint32_t x = 0; x < kSide; ++x) {
      const auto at = y * read.RowBytes() + x * sizeof(uint32_t);
      const std::array expected{std::byte{255}, std::byte{0}, std::byte{0}, std::byte{255}};
      for (size_t c = 0; c < expected.size(); ++c) {
        if ((*pixels)[at + c] != expected[c]) { ++wrongPixels; }
      }
    }
  }
  CHECK(wrongPixels == 0, "the accepted reflected sampler supplies every expected red pixel");
}

void Compute(Device &device) {
  for (const auto &descriptor : ComputeShaders()) {
    Require(CreateComputePipeline(device, descriptor.Id));
  }
  for (const auto id : {ComputeShaderId::Count, std::bit_cast<ComputeShaderId>(~uint32_t{0})}) {
    const auto rejected = CreateComputePipeline(device, id);
    CHECK(!rejected && rejected.error().Message.find("unknown built-in") != std::string::npos,
          "invalid builtin IDs are rejected before reading an artifact");
  }
  const auto *descriptor = FindComputeShader(ComputeShaderId::DepthPyramid);
  CHECK(descriptor != nullptr, "the production depth-pyramid descriptor exists");
  if (descriptor == nullptr) { return; }
  Require(CompileComputePipeline(device, *descriptor));
  for (auto field : {&ComputeShape::Samplers,
                     &ComputeShape::ReadOnlyTextures,
                     &ComputeShape::ReadWriteTextures,
                     &ComputeShape::ReadOnlyBuffers,
                     &ComputeShape::ReadWriteBuffers,
                     &ComputeShape::UniformBuffers,
                     &ComputeShape::GroupX,
                     &ComputeShape::GroupY,
                     &ComputeShape::GroupZ}) {
    auto wrong = *descriptor;
    ++(wrong.Shape.*field);
    const auto pipeline = CompileComputePipeline(device, wrong);
    CHECK(!pipeline && pipeline.error().Kind == ErrorKind::Invalid &&
              pipeline.error().Message.find("declared compute bindings") != std::string::npos,
          "each incorrect compute binding count or workgroup dimension is refused");
  }
}
}

int main() {
  auto device = Require(CreateDevice({.Validation = true}));
  Graphics(*device);
  Compute(*device);
  Covers("native reflected graphics bindings reject missing, extra, moved and mistyped resources; "
         "a correct draw produces expected pixels; every builtin compute shader and altered "
         "binding count/workgroup dimension is checked without the removed SDL compiler API");
  return Report();
}
