#include "Device.h"
#include "Check.h"
#include <algorithm>
#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <ios>
#include <iosfwd>
#include <limits>
#include <memory>
#include <print>
#include <span>
#include <thread>
#include <utility>
#include <vector>

namespace {
using namespace outshine::Render::Native;
using namespace outshine::Test;
constexpr uint32_t kSentinel = 0xdeadbeefu;
constexpr uint64_t kArgumentOffset = 16;
constexpr uint64_t kBufferBytes = 256;

template <class T> T Require(Result<T> value) {
  CHECK(value.has_value(), "native indirect test operation succeeds");
  if (!value) {
    std::println(stderr, "{}", value.error().Message);
    std::_Exit(Report());
  }
  return std::move(*value);
}

void Require(Result<void> value) {
  CHECK(value.has_value(), "native indirect test operation succeeds");
  if (!value) {
    std::println(stderr, "{}", value.error().Message);
    std::_Exit(Report());
  }
}

std::vector<uint32_t> Spirv(const char *path) {
  std::ifstream input(path, std::ios::binary | std::ios::ate);
  const std::streampos bytes = input ? input.tellg() : std::streampos(-1);
  CHECK(bytes > 0 && bytes <= 1048576 && bytes % sizeof(uint32_t) == 0,
        "test shader is bounded and word aligned");
  if (bytes <= 0 || bytes > 1048576 || bytes % sizeof(uint32_t) != 0) { std::_Exit(Report()); }
  std::vector<uint32_t> words(static_cast<size_t>(bytes) / sizeof(uint32_t));
  input.seekg(0);
  input.read(reinterpret_cast<char *>(words.data()), bytes);
  CHECK(input.good(), "test shader bytes are complete");
  return words;
}

Binding Storage(uint32_t set, uint32_t slot, const Buffer &buffer, ByteRange range) {
  return {.Set = set, .Slot = slot, .Value = StorageBuffer{.Data = &buffer, .Range = range}};
}

std::unique_ptr<Buffer>
Upload(Device &gpu, Commands &commands, std::span<const uint32_t> words, bool indirect = false) {
  auto host = Require(gpu.CreateBuffer(
      {.Bytes = words.size_bytes(), .Use = BufferUse::CopySource, .Placement = Memory::Upload}));
  auto use = BufferUse::Storage | BufferUse::CopySource | BufferUse::CopyDestination;
  if (indirect) { use = use | BufferUse::Indirect; }
  auto resident = Require(gpu.CreateBuffer({.Bytes = words.size_bytes(), .Use = use}));
  const auto mapped = Require(gpu.MapWrite(*host, {.Bytes = words.size_bytes()}));
  std::memcpy(mapped.data(), words.data(), words.size_bytes());
  Require(gpu.Unmap(*host));
  Require(commands.Copy(*host, *resident, {.Bytes = words.size_bytes()}));
  return resident;
}

void Completed(Device &gpu, std::unique_ptr<Commands> commands) {
  const auto fence = Require(gpu.Submit(std::move(commands)));
  const auto done = Require(gpu.Wait(fence, std::chrono::seconds(5)));
  CHECK(done == CompletionStatus::Ready, "native work completes within the finite test wait");
}

enum class Chain { SameRecording, AcrossSubmissions, DisjointStorage };

struct Pipelines {
  const ComputePipeline &Producer, &Consumer;
};

void Counts(Device &gpu,
            const Pipelines &pipelines,
            Workgroups groups,
            Chain chain,
            uint64_t argumentOffset = kArgumentOffset) {
  const auto &producer = pipelines.Producer;
  const auto &consumer = pipelines.Consumer;
  const bool sharedBuffer = chain == Chain::DisjointStorage;
  auto commands = Require(gpu.Begin());
  const std::array xyz{
      groups.X, groups.Y, groups.Z, static_cast<uint32_t>(argumentOffset / sizeof(uint32_t))};
  const std::vector<uint32_t> initial(kBufferBytes / sizeof(uint32_t), kSentinel);
  auto input = Upload(gpu, *commands, xyz);
  auto arguments = Upload(gpu, *commands, initial, true);
  const auto output = Upload(gpu, *commands, initial);
  auto readback = Require(gpu.CreateBuffer(
      {.Bytes = kBufferBytes, .Use = BufferUse::CopyDestination, .Placement = Memory::Readback}));
  const std::array writes{Storage(0, 0, *input, {.Bytes = sizeof(xyz)}),
                          Storage(1, 0, *arguments, {.Bytes = kBufferBytes})};
  Require(commands->Dispatch(producer, writes, {}));
  if (chain == Chain::AcrossSubmissions) {
    Completed(gpu, std::move(commands));
    commands = Require(gpu.Begin());
  }
  const auto range =
      sharedBuffer ? ByteRange{.Bytes = argumentOffset} : ByteRange{.Bytes = kBufferBytes};
  const std::array reads{Storage(1, 0, sharedBuffer ? *arguments : *output, range)};
  Require(commands->DispatchIndirect(consumer, reads, *arguments, argumentOffset));
  const auto busy = gpu.MapWrite(*arguments, {.Bytes = kBufferBytes});
  CHECK(!busy, "device-local argument buffers cannot be rewritten by CPU mapping");
  Require(commands->Copy(sharedBuffer ? *arguments : *output, *readback, {.Bytes = kBufferBytes}));
  arguments.reset();
  input.reset();
  Completed(gpu, std::move(commands));
  const auto mapped = Require(gpu.MapRead(*readback, {.Bytes = kBufferBytes}));
  std::vector<uint32_t> actual(initial.size());
  std::memcpy(actual.data(), mapped.data(), mapped.size());
  auto expected = initial;
  const uint32_t invocations = groups.X * groups.Y * groups.Z * consumer.Layout().LocalSize[0];
  for (uint32_t at = 0; at < invocations; ++at) { expected[at] = at + 1; }
  if (sharedBuffer) {
    expected[argumentOffset / sizeof(uint32_t)] = groups.X;
    expected[argumentOffset / sizeof(uint32_t) + 1] = groups.Y;
    expected[argumentOffset / sizeof(uint32_t) + 2] = groups.Z;
  }
  CHECK(actual == expected,
        "GPU-written XYZ drives exact work, zero dimensions preserve sentinels, caller destruction "
        "retains leases");
  Require(gpu.Unmap(*readback));
}

void Refuses(Device &gpu, const ComputePipeline &consumer) {
  auto commands = Require(gpu.Begin());
  auto arguments = Require(
      gpu.CreateBuffer({.Bytes = kBufferBytes, .Use = BufferUse::Indirect | BufferUse::Storage}));
  const auto plain = Require(gpu.CreateBuffer({.Bytes = kBufferBytes, .Use = BufferUse::Storage}));
  const auto shortBuffer = Require(gpu.CreateBuffer({.Bytes = 8, .Use = BufferUse::Indirect}));
  const std::array output{Storage(1, 0, *plain, {.Bytes = kBufferBytes})};
  for (const auto offset :
       {uint64_t{2}, kBufferBytes - 4, std::numeric_limits<uint64_t>::max() - 3}) {
    const auto refused = commands->DispatchIndirect(consumer, output, *arguments, offset);
    CHECK(!refused && refused.error().Kind == ErrorKind::Invalid,
          "misaligned, short and overflowing argument ranges are rejected before dispatch");
  }
  CHECK(!commands->DispatchIndirect(consumer, output, *plain, 0),
        "argument storage must declare indirect usage");
  CHECK(!commands->DispatchIndirect(consumer, output, *shortBuffer, 0),
        "the complete twelve-byte record must fit");
  const std::array overlap{Storage(1, 0, *arguments, {.Bytes = kBufferBytes})};
  CHECK(!commands->DispatchIndirect(consumer, overlap, *arguments, kArgumentOffset),
        "an invocation cannot write over its own indirect argument range");
  auto other = Require(CreateDevice());
  const auto foreign =
      Require(other->CreateBuffer({.Bytes = kBufferBytes, .Use = BufferUse::Indirect}));
  CHECK(!commands->DispatchIndirect(consumer, output, *foreign, 0),
        "foreign-device arguments are rejected");
  const auto foreignPipeline =
      Require(other->CreateComputePipeline(Spirv(OUTSHINE_DISPATCH_COUNT_SHADER)));
  const auto wrongPipeline = commands->DispatchIndirect(*foreignPipeline, output, *arguments, 0);
  CHECK(!wrongPipeline && wrongPipeline.error().Kind == ErrorKind::Invalid,
        "foreign-device pipelines are rejected before indirect encoding");
  bool wrongThreadRejected = false;
  std::thread worker([&] {
    const auto wrongThread = commands->DispatchIndirect(consumer, output, *arguments, 0);
    wrongThreadRejected = !wrongThread && wrongThread.error().Kind == ErrorKind::Invalid;
  });
  worker.join();
  CHECK(wrongThreadRejected, "indirect encoding belongs to the recording thread");
  const auto colour =
      Require(gpu.CreateTexture({.Width = 1, .Height = 1, .Use = TextureUse::ColourAttachment}));
  const std::array attachments{ColourAttachment{
      .Image = {.Image = colour.get()}, .Begin = Load::Clear, .End = Store::Discard}};
  Require(commands->BeginRenderPass({.Colours = attachments, .Depth = {}}));
  const auto inRender = commands->DispatchIndirect(consumer, output, *arguments, 0);
  CHECK(!inRender && inRender.error().Kind == ErrorKind::Invalid,
        "indirect compute cannot interrupt an active render pass");
  Require(commands->EndRenderPass());
}
}

int main() {
  auto gpu = Require(CreateDevice());
  std::println("INDIRECT_DEVICE {}", gpu->Limits().Name);
  const auto producer =
      Require(gpu->CreateComputePipeline(Spirv(OUTSHINE_DISPATCH_ARGUMENTS_SHADER)));
  const auto consumer = Require(gpu->CreateComputePipeline(Spirv(OUTSHINE_DISPATCH_COUNT_SHADER)));
  Counts(*gpu,
         {.Producer = *producer, .Consumer = *consumer},
         {.X = 2, .Y = 3, .Z = 1},
         Chain::SameRecording);
  for (const auto groups : {Workgroups{.X = 0}, Workgroups{.Y = 0}, Workgroups{.Z = 0}}) {
    Counts(*gpu, {.Producer = *producer, .Consumer = *consumer}, groups, Chain::SameRecording);
  }
  Counts(*gpu, {.Producer = *producer, .Consumer = *consumer}, {}, Chain::DisjointStorage);
  Counts(*gpu,
         {.Producer = *producer, .Consumer = *consumer},
         {.X = 2, .Y = 3, .Z = 1},
         Chain::AcrossSubmissions);
  Refuses(*gpu, *consumer);
  for (const auto offset : {uint64_t{4}, kBufferBytes - sizeof(Workgroups)}) {
    Counts(*gpu, {.Producer = *producer, .Consumer = *consumer}, {}, Chain::SameRecording, offset);
  }
  return Report();
}
