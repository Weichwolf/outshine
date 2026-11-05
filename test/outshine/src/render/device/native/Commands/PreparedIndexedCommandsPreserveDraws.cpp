#include "Device.h"
#include "test/harness/render/NativeDeviceCheck.h"
#include <array>
#include <chrono>
#include <cstdint>
#include <cstring>
#include <memory>
#include <span>
#include <vector>

namespace {
using namespace outshine::Render::Native;
using namespace outshine::Test;
using namespace outshine::Test::Gpu;
constexpr uint32_t kRecords = 130;
constexpr uint32_t kWidth = 16;
constexpr uint64_t kBytes = kWidth * kWidth * 4;
constexpr std::array kSpans{IndexedCommandSpan{.First = 1, .Count = 64},
                            IndexedCommandSpan{.First = 65, .Count = 64}};

std::vector<uint32_t> Arguments(uint32_t divisor) {
  std::vector<uint32_t> words(kRecords * 5);
  for (uint32_t at = 1; at < kRecords - 1; ++at) {
    const IndexedIndirectArguments draw{.IndexCount = divisor != 0 && at % divisor == 0 ? 3u : 0u,
                                        .InstanceCount = at % 5 == 0 ? 0u : 1u,
                                        .BaseVertex = -1,
                                        .FirstInstance = at};
    std::memcpy(words.data() + at * 5, &draw, sizeof(draw));
  }
  return words;
}

struct Run {
  bool Prepared = false;
  bool SmallIndices = false;
  bool ChangeAfterPreparation = false;
  bool ReleaseBeforeSubmission = false;
  uint32_t First = 1, Count = 128;
};

std::vector<uint32_t> Render(Device &gpu,
                             const GraphicsPipeline &pipeline,
                             std::unique_ptr<IndexedCommands> &prepared,
                             uint32_t divisor,
                             Run run) {
  auto commands = Require(gpu.Begin());
  const std::array<uint32_t, 4> wide{0xdeadbeefu, 1, 2, 3};
  const std::array<uint32_t, 2> narrow{0x0001ffffu, 0x00030002u};
  auto index =
      Upload(gpu,
             *commands,
             run.SmallIndices ? std::span<const uint32_t>(narrow) : std::span<const uint32_t>(wide),
             BufferUse::Index);
  const IndexBuffer binding{
      .Data = index.get(),
      .Range = {.Offset = run.SmallIndices ? 2u : 4u, .Bytes = run.SmallIndices ? 6u : 12u},
      .Type = run.SmallIndices ? IndexType::Uint16 : IndexType::Uint32};
  auto arguments = Upload(gpu, *commands, Arguments(divisor), BufferUse::Indirect);
  if (run.Prepared) {
    Require(commands->PrepareIndexedCommands(*prepared, binding, *arguments, kSpans));
    CHECK(!commands->PrepareIndexedCommands(*prepared, binding, *arguments, kSpans),
          "one command resource cannot be overwritten in the same recording");
  }
  if (run.ChangeAfterPreparation) {
    const auto words = Arguments(3);
    auto replacement = Require(gpu.CreateBuffer({.Bytes = words.size() * sizeof(uint32_t),
                                                 .Use = BufferUse::CopySource,
                                                 .Placement = Memory::Upload}));
    const auto mapped =
        Require(gpu.MapWrite(*replacement, {.Bytes = replacement->Description().Bytes}));
    std::memcpy(mapped.data(), words.data(), mapped.size());
    Require(gpu.Unmap(*replacement));
    Require(commands->Copy(*replacement, *arguments, {.Bytes = mapped.size()}));
  }
  auto target =
      Require(gpu.CreateTexture({.Width = kWidth,
                                 .Height = kWidth,
                                 .Texels = Format::Rgba8Unorm,
                                 .Use = TextureUse::ColourAttachment | TextureUse::CopySource}));
  auto output = Require(gpu.CreateBuffer(
      {.Bytes = kBytes, .Use = BufferUse::CopyDestination, .Placement = Memory::Readback}));
  const std::array attachments{
      ColourAttachment{.Image = {.Image = target.get()}, .Begin = Load::Clear}};
  Require(commands->BeginRenderPass({.Colours = attachments}));
  Require(commands->DrawIndexedIndirect(
      {.State = {.Pipeline = &pipeline,
                 .Area = {.Width = kWidth, .Height = kWidth},
                 .Clip = {.Width = kWidth, .Height = kWidth}},
       .Indices = binding,
       .Range = {.Data = arguments.get(),
                 .Offset = uint64_t{run.First} * kIndexedIndirectBytes,
                 .Count = run.Count}}));
  Require(commands->EndRenderPass());
  Require(commands->Copy(
      *target, *output, {.RowBytes = kWidth * 4, .Region = {.Width = kWidth, .Height = kWidth}}));
  arguments.reset();
  index.reset();
  if (run.ReleaseBeforeSubmission) { prepared.reset(); }
  const auto completion = Require(gpu.Submit(std::move(commands)));
  CHECK(Require(gpu.Wait(completion, std::chrono::seconds(5))) == CompletionStatus::Ready,
        "prepared native draws finish within the explicit wait");
  const auto mapped = Require(gpu.MapRead(*output, {.Bytes = kBytes}));
  std::vector<uint32_t> pixels(kWidth * kWidth);
  std::memcpy(pixels.data(), mapped.data(), mapped.size());
  Require(gpu.Unmap(*output));
  return pixels;
}

void Refuses(Device &gpu, IndexedCommands &prepared) {
  auto commands = Require(gpu.Begin());
  const std::array<uint32_t, 4> words{0, 1, 2, 3};
  auto index = Upload(gpu, *commands, words, BufferUse::Index);
  auto arguments = Upload(gpu, *commands, Arguments(1), BufferUse::Indirect);
  const IndexBuffer binding{.Data = index.get(), .Range = {.Bytes = sizeof(words)}};
  for (const auto span : {IndexedCommandSpan{.First = 1, .Count = 130},
                          IndexedCommandSpan{.First = 1, .Count = 0},
                          IndexedCommandSpan{.First = 0xffffffffu, .Count = 2}}) {
    CHECK(!commands->PrepareIndexedCommands(prepared, binding, *arguments, std::span(&span, 1)),
          "empty and overflowing spans fail before GPU encoding");
  }
  const std::array overlap{IndexedCommandSpan{.First = 1, .Count = 64},
                           IndexedCommandSpan{.First = 32, .Count = 64}};
  CHECK(!commands->PrepareIndexedCommands(prepared, binding, *arguments, overlap),
        "overlapping spans cannot overwrite each other's GPU command storage");
  auto other = Require(CreateDevice());
  auto foreign = Require(other->CreateBuffer({.Bytes = sizeof(words), .Use = BufferUse::Index}));
  CHECK(
      !commands->PrepareIndexedCommands(
          prepared, {.Data = foreign.get(), .Range = {.Bytes = sizeof(words)}}, *arguments, kSpans),
      "prepared indices belong to their recording device");
}
}

int main() {
  auto gpu = Require(CreateDevice());
  if (!gpu->Limits().PreparedIndexedCommands) {
    const auto unsupported = gpu->CreateIndexedCommands(kRecords);
    CHECK(!unsupported && unsupported.error().Kind == ErrorKind::Unsupported,
          "backends with native multi-draw retain their ordinary indirect path");
    return Report();
  }
  CHECK(!gpu->CreateIndexedCommands(0), "zero command capacity is invalid");
  const auto vertex = Spirv(OUTSHINE_INDEXED_VERTEX_SHADER);
  const auto fragment = Spirv(OUTSHINE_INDEXED_FRAGMENT_SHADER);
  const std::array colours{ColourTarget{.Texels = Format::Rgba8Unorm}};
  const auto pipeline =
      Require(gpu->CreateGraphicsPipeline({.Vertex = {.Spirv = vertex},
                                           .Fragment = ShaderCode{.Spirv = fragment},
                                           .Colours = colours,
                                           .Culling = Cull::None,
                                           .PreparedIndexedCommands = true}));
  auto prepared = Require(gpu->CreateIndexedCommands(kRecords));
  CHECK(prepared->AllocatedBytes() > 0, "native command residency is observable");
  Refuses(*gpu, *prepared);
  for (const bool narrow : {false, true}) {
    for (const uint32_t divisor : {1u, 64u, 0u, 7u, 0u, 1u}) {
      const auto direct = Render(*gpu, *pipeline, prepared, divisor, {.SmallIndices = narrow});
      const auto compact =
          Render(*gpu, *pipeline, prepared, divisor, {.Prepared = true, .SmallIndices = narrow});
      CHECK(
          compact == direct,
          "dense, sparse and empty command reuse preserves pixel order, base vertex and instance");
      CHECK(direct.front() != 0 || divisor == 0, "nonempty indirect draws produce actual pixels");
    }
  }
  for (const auto span : {IndexedCommandSpan{.First = 14, .Count = 115},
                          IndexedCommandSpan{.First = 1, .Count = 117},
                          IndexedCommandSpan{.First = 14, .Count = 60}}) {
    CHECK(Render(*gpu, *pipeline, prepared, 1, {.First = span.First, .Count = span.Count}) ==
              Render(*gpu,
                     *pipeline,
                     prepared,
                     1,
                     {.Prepared = true, .First = span.First, .Count = span.Count}),
          "unaligned ranges preserve CPU edges and GPU-compacted interior command order");
  }
  CHECK(Render(*gpu, *pipeline, prepared, 1, {.Prepared = true, .ChangeAfterPreparation = true}) ==
            Render(*gpu, *pipeline, prepared, 3, {}),
        "argument mutation invalidates prepared commands and reads the new draw values");
  CHECK(
      Render(*gpu, *pipeline, prepared, 1, {}) ==
          Render(*gpu, *pipeline, prepared, 1, {.Prepared = true, .ReleaseBeforeSubmission = true}),
      "submission retains command, index and argument resources after caller destruction");
  return Report();
}
