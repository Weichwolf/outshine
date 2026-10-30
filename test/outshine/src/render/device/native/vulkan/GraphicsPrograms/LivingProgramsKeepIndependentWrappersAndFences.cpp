#include "Device.h"
#include "src/render/device/native/vulkan/VulkanGraphics.h"
#include "Check.h"
#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <dlfcn.h>
#include <fstream>
#include <ios>
#include <iosfwd>
#include <memory>
#include <print>
#include <span>
#include <thread>
#include <utility>
#include <vector>

namespace {
using namespace outshine::Render::Native;
using namespace outshine::Render::Native::Vulkan;
using namespace outshine::Test;
size_t pipelineCalls = 0;

template <class T> T Require(Result<T> result) {
  CHECK(result, "native graphics operation succeeds");
  if (!result) {
    std::println(stderr, "{}", result.error().Message);
    std::_Exit(Report());
  }
  return std::move(*result);
}

void Require(Result<void> result) {
  CHECK(result, "native graphics operation succeeds");
  if (!result) {
    std::println(stderr, "{}", result.error().Message);
    std::_Exit(Report());
  }
}

std::vector<uint32_t> Spirv(const char *path) {
  std::ifstream file(path, std::ios::binary | std::ios::ate);
  const auto bytes = file ? file.tellg() : std::streampos(-1);
  CHECK(bytes > 0 && bytes <= 1048576 && bytes % sizeof(uint32_t) == 0,
        "the independent test shader is bounded and word aligned");
  if (bytes <= 0 || bytes > 1048576 || bytes % sizeof(uint32_t) != 0) { std::_Exit(Report()); }
  std::vector<uint32_t> words(static_cast<size_t>(bytes) / sizeof(uint32_t));
  file.seekg(0);
  file.read(reinterpret_cast<char *>(words.data()), bytes);
  CHECK(file.good(), "the independent shader is read completely");
  return words;
}

std::shared_ptr<PipelineState> State(const GraphicsPipeline &pipeline) {
  const auto *native = dynamic_cast<const VulkanGraphicsPipeline *>(&pipeline);
  CHECK(native != nullptr, "the target test uses the Vulkan backend");
  if (native == nullptr) { std::_Exit(Report()); }
  return native->State()->Pipeline;
}

std::unique_ptr<Commands> Record(Device &device, GraphicsPipeline &pipeline, Texture &target) {
  auto commands = Require(device.Begin());
  const std::array colours{ColourAttachment{.Image = {.Image = &target}, .Begin = Load::Clear}};
  Require(commands->BeginRenderPass({.Colours = colours, .Depth = {}}));
  const std::array<float, 4> tint{1, 1, 1, 1};
  const std::array bindings{
      Binding{.Set = 0, .Slot = 0, .Value = Uniform{.Bytes = std::as_bytes(std::span(tint))}}};
  Require(commands->Draw({.State = {.Pipeline = &pipeline,
                                    .Bindings = bindings,
                                    .Vertices = {},
                                    .Area = {.Width = 2, .Height = 2},
                                    .Clip = {.Width = 2, .Height = 2}},
                          .Range = {.Count = 3}}));
  Require(commands->EndRenderPass());
  return commands;
}

void Complete(Device &device, std::unique_ptr<Commands> commands, Texture &target) {
  auto buffer = Require(device.CreateBuffer(
      {.Bytes = 16, .Use = BufferUse::CopyDestination, .Placement = Memory::Readback}));
  Require(commands->Copy(target, *buffer, {.RowBytes = 8, .Region = {.Width = 2, .Height = 2}}));
  const auto completion = Require(device.Submit(std::move(commands)));
  CHECK(Require(device.Wait(completion, std::chrono::seconds(5))) == CompletionStatus::Ready,
        "the actual drawing and readback finish before retirement");
  const auto pixels = Require(device.MapRead(*buffer, {.Bytes = 16}));
  for (size_t at = 0; at < 16; at += 4) {
    CHECK(pixels[at + 2] == std::byte{64} && pixels[at + 3] == std::byte{255} &&
              pixels[at] != std::byte{0} && pixels[at + 1] != std::byte{0},
          "the cached program draws its independent shader and reflected uniform layout");
  }
  Require(device.Unmap(*buffer));
}

void WrappersAndRecordings(Device &device, const GraphicsDescription &description) {
  const auto before = pipelineCalls;
  auto first = Require(device.CreateGraphicsPipeline(description));
  auto alias = Require(device.CreateGraphicsPipeline(description));
  CHECK(first.get() != alias.get() && State(*first) == State(*alias) && pipelineCalls == before + 1,
        "independent API wrappers share one actual Vulkan program");
  CHECK(alias->Layout().Bindings.size() == 1 &&
            alias->Layout().Bindings[0].Resource.Kind == BindingKind::Uniform &&
            alias->Layout().Bindings[0].Resource.MinimumBytes == 16,
        "a hit owns the original complete reflected layout");
  auto target = Require(device.CreateTexture(
      {.Width = 2, .Height = 2, .Use = TextureUse::ColourAttachment | TextureUse::CopySource}));
  auto recorded = Record(device, *first, *target);
  const std::weak_ptr<PipelineState> old = State(*first);
  first.reset();
  alias.reset();
  CHECK(!old.expired(), "recorded work retains the native program without any API wrapper");
  auto survivor = Require(device.CreateGraphicsPipeline(description));
  CHECK(State(*survivor) == old.lock() && pipelineCalls == before + 1,
        "an outstanding recording also supplies a reusable living program");
  Complete(device, std::move(recorded), *target);
  survivor.reset();
  CHECK(old.expired(), "the weak cache does not retain the program beyond its last completed use");
  auto replacement = Require(device.CreateGraphicsPipeline(description));
  CHECK(pipelineCalls == before + 2, "an expired program is actually rebuilt on the next demand");
}

void Boundaries(Device &device, const GraphicsDescription &description) {
  auto first = Require(device.CreateGraphicsPipeline(description));
  auto context = State(*first)->Owner;
  auto code =
      std::vector<uint32_t>(description.Vertex.Spirv.begin(), description.Vertex.Spirv.end());
  auto copied = description;
  copied.Vertex.Spirv = code;
  auto equal = Require(device.CreateGraphicsPipeline(copied));
  CHECK(State(*equal) == State(*first),
        "recipe identity uses shader bytes rather than their address");
  code[0] ^= 1;
  const auto beforeMutation = pipelineCalls;
  CHECK(!device.CreateGraphicsPipeline(copied) && pipelineCalls == beforeMutation,
        "changed caller storage cannot reuse its previously valid shader snapshot");
  GraphicsPrograms programs(context);
  for (size_t at = 0; at < 8; ++at) {
    auto program = Require(programs.Create(description));
    CHECK(programs.StoredPrograms() == 1 && programs.RecipeCapacityBytes() > 0,
          "sequential dead recipes do not accumulate CPU entries");
  }
  const auto calls = pipelineCalls;
  auto invalid = description;
  invalid.Vertex.Entry = "missing-entry";
  CHECK(!programs.Create(invalid) && pipelineCalls == calls && programs.StoredPrograms() == 0,
        "failed reflection is not cached and expired metadata is retired");
  auto other = Require(CreateDevice({.Validation = true}));
  auto isolated = Require(other->CreateGraphicsPipeline(description));
  CHECK(State(*isolated)->Owner != context && State(*isolated) != State(*first),
        "different native devices never share programs");
  std::thread foreign([&] {
    const auto rejected = programs.Create(description);
    CHECK(!rejected && rejected.error().Kind == ErrorKind::Invalid,
          "wrong-thread requests are rejected before touching compiler state");
  });
  foreign.join();
}
}

extern "C" VKAPI_ATTR VkResult VKAPI_CALL
vkCreateGraphicsPipelines(VkDevice device,
                          VkPipelineCache cache,
                          uint32_t count,
                          const VkGraphicsPipelineCreateInfo *pCreateInfos,
                          const VkAllocationCallbacks *allocator,
                          VkPipeline *pipelines) {
  static const auto call = reinterpret_cast<PFN_vkCreateGraphicsPipelines>(
      dlsym(RTLD_NEXT, "vkCreateGraphicsPipelines"));
  if (call == nullptr) { std::abort(); }
  pipelineCalls += count;
  return call(device, cache, count, pCreateInfos, allocator, pipelines);
}

int main() {
  auto device = Require(CreateDevice({.Validation = true}));
  const auto vertex = Spirv(OUTSHINE_GRAPHICS_VERTEX);
  const auto fragment = Spirv(OUTSHINE_GRAPHICS_FRAGMENT);
  const std::array colours{ColourTarget{}};
  const GraphicsDescription description{.Vertex = {.Spirv = vertex},
                                        .Fragment = ShaderCode{.Spirv = fragment},
                                        .Streams = {},
                                        .Attributes = {},
                                        .Colours = colours,
                                        .Depth = {},
                                        .Culling = Cull::None,
                                        .Label = "independent graphics fixture"};
  WrappersAndRecordings(*device, description);
  Boundaries(*device, description);
  Covers("real graphics program reuse, independent reflected layouts, pending recording and "
         "fence retirement, expired recipes, failure, device and owner-thread isolation");
  return Report();
}
