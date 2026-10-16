#include "NativeGpuCheck.h"
#include "src/render/device/ReadbackTransfer.h"
#include "src/render/device/native/vulkan/Recording.h"
#include "Check.h"
#include <algorithm>
#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <memory>
#include <print>
#include <span>
#include <string_view>
#include <utility>
#include <vector>

namespace {
using namespace outshine::Render;
using namespace outshine::Render::Native;
using namespace outshine::Render::Native::Vulkan;
using namespace outshine::Test;
using outshine::Test::Gpu::Require;
using outshine::Test::Gpu::Spirv;
constexpr uint32_t kWidth = 64;
constexpr uint32_t kHeight = 16;
constexpr uint32_t kPixels = kWidth * kHeight;
constexpr uint32_t kRepeats = 3;
constexpr uint32_t kGreen = 7;
constexpr uint32_t kBlue = 13;
constexpr uint32_t kBias = 17;
constexpr uint32_t kSourceMultiplier = 3;
constexpr auto kWait = std::chrono::seconds(5);

struct alignas(16) Payload {
  std::array<uint32_t, 4> Words{};
};

static_assert(sizeof(Payload) == 16 && alignof(Payload) == 16);

Binding PayloadAt(uint32_t set, const Payload &payload) {
  return {.Set = set, .Slot = 0, .Value = Uniform{.Bytes = std::as_bytes(std::span(&payload, 1))}};
}

Binding BufferAt(uint32_t set, const Buffer &buffer) {
  return {.Set = set,
          .Slot = 0,
          .Value = StorageBuffer{.Data = &buffer, .Range = {.Bytes = buffer.Description().Bytes}}};
}

struct SourcePattern {
  uint32_t Count;
  uint32_t Bias;
};

std::unique_ptr<Buffer> Source(Device &device, SourcePattern pattern) {
  auto buffer = Require(device.CreateBuffer(
      {.Bytes = uint64_t{pattern.Count} * sizeof(uint32_t),
       .Use = BufferUse::Storage | BufferUse::CopySource | BufferUse::CopyDestination,
       .Placement = Memory::Upload}));
  auto mapped = Require(device.MapWrite(*buffer, {.Bytes = buffer->Description().Bytes}));
  for (uint32_t at = 0; at < pattern.Count; ++at) {
    const uint32_t value = at * kSourceMultiplier + pattern.Bias;
    std::memcpy(mapped.data() + at * sizeof(uint32_t), &value, sizeof(value));
  }
  Require(device.Unmap(*buffer));
  return buffer;
}

std::vector<uint32_t> Words(Device &device, const Buffer &buffer) {
  auto read =
      Require(ReadbackTransfer::Start(device, buffer, {.Bytes = buffer.Description().Bytes}));
  auto data = Require(read.Wait(device, kWait));
  CHECK(data, "the independently copied output completes");
  if (!data) { outshine::Test::Gpu::Failed(); }
  std::vector<uint32_t> words(buffer.Description().Bytes / sizeof(uint32_t));
  std::memcpy(words.data(), data->data(), buffer.Description().Bytes);
  return words;
}

void DispatchMany(Commands &commands,
                  const ComputePipeline &pipeline,
                  const Buffer &source,
                  const Buffer &target,
                  uint32_t count) {
  for (uint32_t at = 0; at < count; ++at) {
    Payload payload{{at, kBias, 0, 0}};
    const std::array bindings{BufferAt(0, source), PayloadAt(1, payload), BufferAt(2, target)};
    Require(commands.Dispatch(pipeline, bindings, {}));
    payload.Words.fill(~uint32_t{0});
  }
}

void Compute(Device &device, const ComputePipeline &pipeline) {
  auto source = Source(device, {.Count = kPixels, .Bias = 0});
  auto replacement = Source(device, {.Count = kPixels, .Bias = kBias});
  auto target = Require(device.CreateBuffer({.Bytes = uint64_t{kPixels} * sizeof(uint32_t),
                                             .Use = BufferUse::Storage | BufferUse::CopySource}));
  auto commands = Require(device.Begin());
  auto *recording = dynamic_cast<Recording *>(commands.get());
  CHECK(recording, "the Vulkan probe observes the real recording costs");
  if (recording == nullptr) { outshine::Test::Gpu::Failed(); }
  DispatchMany(*commands, pipeline, *source, *target, kPixels);
  Require(commands->Copy(*replacement, *source, {.Bytes = source->Description().Bytes}));
  DispatchMany(*commands, pipeline, *source, *target, kPixels);
  const auto costs = recording->BindingCosts();
  CHECK(costs.Sets == kPixels + 2 && costs.SetRequests == uint64_t{kPixels} * 6,
        "unchanged input/output sets are reused while every distinct uniform set remains present");
  CHECK(costs.UniformRequests == uint64_t{kPixels} * 2 && costs.UniformHits == kPixels,
        "uniform snapshots own reused stack bytes instead of using caller pointer identity");
  CHECK(costs.UniformUploadBytes == uint64_t{kPixels} * sizeof(Payload) &&
            costs.UniformSnapshotBytes == costs.UniformUploadBytes,
        "only distinct immutable uniform values are copied and retained");
  CHECK(costs.Pools > 1 && costs.Sets > 128 && costs.UniformPages < kPixels,
        "the real workload crosses the old pool with bounded pages and packed uniform allocations");
  const auto done = Require(device.Submit(std::move(commands)));
  CHECK(Require(device.Wait(done, kWait)) == CompletionStatus::Ready,
        "compute submission completes");
  const auto output = Words(device, *target);
  for (uint32_t at = 0; at < kPixels; ++at) {
    CHECK(output[at] == at * kSourceMultiplier + 2 * kBias,
          "cached descriptors retain write/read barriers after the intervening GPU copy");
  }
  std::println(
      "COMPUTE requests={} sets={} hits={} pools={} uniform_pages={} capacity={} copied={}",
      costs.SetRequests,
      costs.Sets,
      costs.SetHits,
      costs.Pools,
      costs.UniformPages,
      costs.UniformCapacityBytes,
      costs.UniformUploadBytes);
}

std::unique_ptr<Texture> White(Device &device) {
  constexpr std::array<float, 4> white{1, 1, 1, 1};
  auto staging = Require(device.CreateBuffer(
      {.Bytes = sizeof(white), .Use = BufferUse::CopySource, .Placement = Memory::Upload}));
  auto mapped = Require(device.MapWrite(*staging, {.Bytes = sizeof(white)}));
  std::memcpy(mapped.data(), white.data(), sizeof(white));
  Require(device.Unmap(*staging));
  auto image =
      Require(device.CreateTexture({.Width = 1,
                                    .Height = 1,
                                    .Texels = Format::Rgba32Float,
                                    .Use = TextureUse::Sampled | TextureUse::CopyDestination}));
  auto commands = Require(device.Begin());
  Require(commands->Copy(
      *staging, *image, {.RowBytes = sizeof(white), .Region = {.Width = 1, .Height = 1}}));
  const auto done = Require(device.Submit(std::move(commands)));
  CHECK(Require(device.Wait(done, kWait)) == CompletionStatus::Ready,
        "the sampled fixture completes");
  return image;
}

void Raster(Device &device, const GraphicsPipeline &pipeline) {
  auto image = White(device);
  auto sampler = Require(device.CreateSampler(
      {.Min = Filter::Nearest, .Mag = Filter::Nearest, .Mip = Filter::Nearest}));
  auto target =
      Require(device.CreateTexture({.Width = kWidth,
                                    .Height = kHeight,
                                    .Texels = Format::Rgba32Float,
                                    .Use = TextureUse::ColourAttachment | TextureUse::CopySource}));
  auto commands = Require(device.Begin());
  const std::array colours{
      ColourAttachment{.Image = {.Image = target.get()}, .Begin = Load::Clear}};
  Require(commands->BeginRenderPass({.Colours = colours, .Depth = {}}));
  const Payload view{};
  for (uint32_t at = 0; at < kPixels; ++at) {
    Payload object{{at, kGreen, kBlue, 0}};
    const std::array bindings{PayloadAt(0, view),
                              PayloadAt(1, object),
                              Binding{.Set = 2,
                                      .Slot = 0,
                                      .Value = SampledTexture{.Image = {.Image = image.get()},
                                                              .Sampling = sampler.get()}}};
    for (uint32_t repeat = 0; repeat < kRepeats; ++repeat) {
      Require(commands->Draw(
          {.State = {.Pipeline = &pipeline,
                     .Bindings = bindings,
                     .Vertices = {},
                     .Area = {.Width = kWidth, .Height = kHeight},
                     .Clip = {.X = at % kWidth, .Y = at / kWidth, .Width = 1, .Height = 1}},
           .Range = {.Count = 3}}));
    }
    object.Words.fill(~uint32_t{0});
  }
  Require(commands->EndRenderPass());
  auto *recording = dynamic_cast<Recording *>(commands.get());
  CHECK(recording, "the raster probe observes the same native allocation path");
  if (recording == nullptr) { outshine::Test::Gpu::Failed(); }
  const auto costs = recording->BindingCosts();
  CHECK(costs.Sets == kPixels + 2 && costs.SetRequests == uint64_t{kPixels} * kRepeats * 3,
        "material and view sets survive thousands of repeated native draws without reallocation");
  CHECK(costs.UniformUploadBytes == uint64_t{kPixels + 1} * sizeof(Payload),
        "only the distinct object and view snapshots enter the packed upload pages");
  CHECK(costs.Views == 2 && costs.ViewHits == uint64_t{kPixels} * kRepeats - 1,
        "the sampled image view is reused and its attachment view stays separately owned");
  image.reset();
  sampler.reset();
  const auto done = Require(device.Submit(std::move(commands)));
  CHECK(Require(device.Wait(done, kWait)) == CompletionStatus::Ready,
        "the large raster recording completes");
  auto read =
      Require(ReadbackTransfer::Start(device, *target, {.Width = kWidth, .Height = kHeight}));
  const auto data = Require(read.Wait(device, kWait));
  CHECK(data, "all actual raster pixels are independently readable");
  if (!data) { outshine::Test::Gpu::Failed(); }
  for (uint32_t y = 0; y < kHeight; ++y) {
    for (uint32_t x = 0; x < kWidth; ++x) {
      std::array<float, 4> pixel{};
      std::memcpy(
          pixel.data(), data->data() + y * read.RowBytes() + x * sizeof(pixel), sizeof(pixel));
      CHECK(pixel == (std::array<float, 4>{static_cast<float>(y * kWidth + x), kGreen, kBlue, 1}),
            "every draw keeps its own uniform snapshot after the caller overwrites the source");
    }
  }
  std::println(
      "RASTER requests={} sets={} hits={} pools={} uniform_pages={} capacity={} copied={} views={}",
      costs.SetRequests,
      costs.Sets,
      costs.SetHits,
      costs.Pools,
      costs.UniformPages,
      costs.UniformCapacityBytes,
      costs.UniformUploadBytes,
      costs.Views);
}

void Lifetime(Device &device, const ComputePipeline &pipeline, bool validation) {
  constexpr uint32_t count = 16;
  constexpr uint32_t initial = 1000;
  auto target = Source(device, {.Count = count, .Bias = initial});
  for (const bool foreign : {false, true}) {
    auto source = Source(device, {.Count = count, .Bias = 0});
    const auto *native = dynamic_cast<const VulkanBuffer *>(source.get());
    CHECK(native, "the actual input allocation is observable for lifetime checking");
    if (native == nullptr) { outshine::Test::Gpu::Failed(); }
    const std::weak_ptr<BufferState> weak = native->State();
    auto commands = Require(device.Begin());
    DispatchMany(*commands, pipeline, *source, *target, count);
    source.reset();
    CHECK(!weak.expired(), "recorded storage survives release of the public input handle");
    if (foreign) {
      auto other = Require(CreateDevice({.Validation = validation}));
      auto rejected = other->Submit(std::move(commands));
      CHECK(!rejected && rejected.error().Kind == ErrorKind::Invalid,
            "foreign submission rejects and consumes the owned recording");
    } else {
      commands.reset();
    }
    CHECK(weak.expired(),
          "cancelled or rejected recordings release their retained input allocation");
    const auto output = Words(device, *target);
    for (uint32_t at = 0; at < count; ++at) {
      CHECK(output[at] == at * kSourceMultiplier + initial,
            "cancelled descriptor recordings cannot modify previously accepted output bytes");
    }
  }
  auto source = Source(device, {.Count = count, .Bias = 0});
  const auto *native = dynamic_cast<const VulkanBuffer *>(source.get());
  CHECK(native, "the accepted recording uses the same native allocation type");
  if (native == nullptr) { outshine::Test::Gpu::Failed(); }
  const std::weak_ptr<BufferState> weak = native->State();
  auto commands = Require(device.Begin());
  DispatchMany(*commands, pipeline, *source, *target, count);
  source.reset();
  const auto done = Require(device.Submit(std::move(commands)));
  CHECK(!weak.expired(), "accepted work retains its input until actual completion is reaped");
  CHECK(Require(device.Wait(done, kWait)) == CompletionStatus::Ready,
        "the accepted lifetime probe completes within the explicit offline budget");
  CHECK(weak.expired(), "completed descriptor work does not archive GPU input resources");
  const auto output = Words(device, *target);
  for (uint32_t at = 0; at < count; ++at) {
    CHECK(output[at] == at * kSourceMultiplier + kBias,
          "retained resources and uniform snapshots reach the GPU before their final release");
  }
}

void Bounded(Device &device, const ComputePipeline &pipeline) {
  constexpr uint32_t count = static_cast<uint32_t>(BindingStorage::kSetsMost) + 1;
  constexpr uint32_t initial = 1000;
  auto source = Source(device, {.Count = count, .Bias = 0});
  auto target = Source(device, {.Count = count, .Bias = initial});
  auto commands = Require(device.Begin());
  uint32_t accepted = 0;
  bool rejected = false;
  for (uint32_t at = 0; at < count; ++at) {
    const Payload payload{{at, kBias, 0, 0}};
    const std::array bindings{BufferAt(0, *source), PayloadAt(1, payload), BufferAt(2, *target)};
    auto step = commands->Dispatch(pipeline, bindings, {});
    if (!step) {
      CHECK(step.error().Kind == ErrorKind::Busy,
            "the bounded recording budget returns recoverable backpressure");
      rejected = true;
      break;
    }
    ++accepted;
  }
  CHECK(rejected && accepted > 128 && accepted < count,
        "genuine new bindings cross the old pool and stop at the finite recording capacity");
  if (accepted == 0) { outshine::Test::Gpu::Failed(); }
  auto *recording = dynamic_cast<Recording *>(commands.get());
  CHECK(recording, "bounded descriptor costs belong to the actual Vulkan recording");
  if (recording == nullptr) { outshine::Test::Gpu::Failed(); }
  const auto costs = recording->BindingCosts();
  CHECK(costs.Sets <= BindingStorage::kSetsMost && costs.Pools <= BindingStorage::kPoolsMost &&
            costs.UniformCapacityBytes <= BindingStorage::kUniformCapacityMost,
        "descriptor and uniform capacities remain independently bounded");
  const Payload previous{{accepted - 1, kBias, 0, 0}};
  const std::array bindings{BufferAt(0, *source), PayloadAt(1, previous), BufferAt(2, *target)};
  Require(commands->Dispatch(pipeline, bindings, {}));
  CHECK(recording->BindingCosts().Sets == costs.Sets,
        "previously accepted descriptors remain usable when new allocations are refused");
  const auto done = Require(device.Submit(std::move(commands)));
  CHECK(Require(device.Wait(done, kWait)) == CompletionStatus::Ready,
        "the valid bounded prefix can still be submitted");
  const auto output = Words(device, *target);
  for (uint32_t at = 0; at < count; ++at) {
    CHECK(output[at] == at * kSourceMultiplier + (at < accepted ? kBias : initial),
          "the exact accepted prefix executes while rejected and untouched indices retain their "
          "data");
  }
  std::println("BOUNDED accepted={} sets={} pools={} uniform_capacity={}",
               accepted,
               costs.Sets,
               costs.Pools,
               costs.UniformCapacityBytes);
}

}

int main(int argc, char **argv) {
  const bool validation = argc == 5 && std::string_view(argv[4]) == "--validation";
  if (argc != 4 && !validation) { return 2; }
  auto device = Require(CreateDevice({.Validation = validation}));
  const auto vertex = Spirv(argv[1]);
  const auto fragment = Spirv(argv[2]);
  const auto compute = Spirv(argv[3]);
  const std::array targets{ColourTarget{.Texels = Format::Rgba32Float, .Rgb = {}, .Alpha = {}}};
  auto graphics = Require(device->CreateGraphicsPipeline({.Vertex = {.Spirv = vertex},
                                                          .Fragment = ShaderCode{.Spirv = fragment},
                                                          .Streams = {},
                                                          .Attributes = {},
                                                          .Colours = targets,
                                                          .Depth = {},
                                                          .Culling = Cull::None,
                                                          .Label = "descriptor reuse"}));
  auto pipeline = Require(device->CreateComputePipeline(compute));
  Compute(*device, *pipeline);
  Raster(*device, *graphics);
  Lifetime(*device, *pipeline, validation);
  Bounded(*device, *pipeline);
  return Report();
}
