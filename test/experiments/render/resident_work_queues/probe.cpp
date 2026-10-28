#include "Device.h"
#include "Check.h"
#include <algorithm>
#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <ios>
#include <iosfwd>
#include <memory>
#include <print>
#include <span>
#include <string>
#include <utility>
#include <vector>

namespace {
using namespace outshine::Render::Native;
using namespace outshine::Test;
constexpr uint32_t kChunk = 64;
constexpr std::array<uint32_t, 19> kCounts{
    301, 1, 399, 0, 31, 32, 33, 63, 64, 65, 127, 128, 129, 255, 256, 257, 1939, 7, 7};

struct alignas(16) Root {
  uint32_t First, Count, Logical, Visibility;
};

struct Chunk {
  uint32_t Root, First;
  bool operator==(const Chunk &) const = default;
};

struct alignas(16) View {
  uint32_t Roots, Capacity, ChunkSize, Unused;
};

static_assert(sizeof(Root) == 16 && alignof(Root) == 16);
static_assert(sizeof(Chunk) == 8 && offsetof(Chunk, First) == 4);
static_assert(sizeof(View) == 16 && offsetof(View, ChunkSize) == 8);

std::vector<uint32_t> Spirv(const std::string &path) {
  std::ifstream file(path, std::ios::binary | std::ios::ate);
  const auto size = file ? file.tellg() : std::streampos(-1);
  if (size <= 0 || size > 1048576 || size % 4 != 0) { return {}; }
  std::vector<uint32_t> result(static_cast<size_t>(size) / 4);
  file.seekg(0);
  file.read(reinterpret_cast<char *>(result.data()), size);
  return file ? result : std::vector<uint32_t>{};
}

std::unique_ptr<Buffer>
Upload(Device &device, Commands &commands, std::span<const std::byte> bytes) {
  auto host = device.CreateBuffer(
      {.Bytes = bytes.size(), .Use = BufferUse::CopySource, .Placement = Memory::Upload});
  auto gpu = device.CreateBuffer(
      {.Bytes = bytes.size(),
       .Use = BufferUse::Storage | BufferUse::CopyDestination | BufferUse::CopySource});
  CHECK(host && gpu, "bounded worklist inputs allocate");
  if (!host || !gpu) { return {}; }
  auto mapped = device.MapWrite(**host, {.Bytes = bytes.size()});
  CHECK(mapped.has_value(), "input range maps for the upload");
  if (!mapped) { return {}; }
  std::ranges::copy(bytes, mapped->begin());
  CHECK(device.Unmap(**host).has_value(), "input upload is released");
  CHECK(commands.Copy(**host, **gpu, {.Bytes = bytes.size()}).has_value(),
        "native copy retains upload lifetime");
  return std::move(*gpu);
}

Binding Storage(uint32_t set, uint32_t slot, const Buffer &buffer) {
  return {.Set = set,
          .Slot = slot,
          .Value = StorageBuffer{.Data = &buffer, .Range = {.Bytes = buffer.Description().Bytes}}};
}

struct Kernels {
  const ComputePipeline &Reset, &Expand;
};

struct StorageBuffers {
  std::unique_ptr<Buffer> State, Queue;
};

std::vector<std::byte> Execute(Device &device,
                               const Kernels &kernels,
                               const StorageBuffers &storage,
                               std::span<const Root> roots,
                               View view,
                               bool omitReset) {
  auto recording = device.Begin();
  if (!recording) { return {}; }
  auto inputs = Upload(device, **recording, std::as_bytes(std::span(roots)));
  if (!inputs) { return {}; }
  const std::array resetBindings{Storage(0, 0, *storage.State)};
  if (!omitReset) {
    CHECK((*recording)->Dispatch(kernels.Reset, resetBindings, {}).has_value(),
          "GPU reset precedes every expansion");
  }
  const std::array bindings{
      Storage(0, 0, *inputs),
      Storage(1, 0, *storage.Queue),
      Storage(1, 1, *storage.State),
      Binding{.Set = 2, .Slot = 0, .Value = Uniform{.Bytes = std::as_bytes(std::span(&view, 1))}}};
  CHECK((*recording)->Dispatch(kernels.Expand, bindings, {.X = (view.Roots + 63) / 64}).has_value(),
        "only roots are directly dispatched");
  auto result = device.CreateBuffer(
      {.Bytes = storage.State->Description().Bytes + storage.Queue->Description().Bytes,
       .Use = BufferUse::CopyDestination,
       .Placement = Memory::Readback});
  if (!result) { return {}; }
  CHECK((*recording)
            ->Copy(*storage.State, **result, {.Bytes = storage.State->Description().Bytes})
            .has_value(),
        "test reads completed queue counters");
  CHECK((*recording)
            ->Copy(*storage.Queue,
                   **result,
                   {.DestinationOffset = storage.State->Description().Bytes,
                    .Bytes = storage.Queue->Description().Bytes})
            .has_value(),
        "test reads bounded queue and sentinels");
  const auto completion = device.Submit(std::move(*recording));
  if (!completion) { return {}; }
  const auto waited = device.Wait(*completion, std::chrono::seconds(5));
  if (!waited || *waited != CompletionStatus::Ready) { return {}; }
  const auto mapped = device.MapRead(**result, {.Bytes = (*result)->Description().Bytes});
  if (!mapped) { return {}; }

  std::vector<std::byte> bytes(mapped->begin(), mapped->end());
  CHECK(device.Unmap(**result).has_value(), "test readback releases ownership");
  return bytes;
}

struct Expected {
  std::vector<Chunk> Entries;
  uint32_t Expanded = 0;
};

Expected Enumerate(unsigned scenario, std::span<Root> roots, uint32_t maximumChunks) {
  std::vector<Chunk> expected;
  expected.reserve(maximumChunks);
  uint32_t expanded = 0;
  for (uint32_t root = 0; root < roots.size(); ++root) {
    auto &value = roots[root];
    value.Visibility = 1;
    if (scenario == 1) { value.Visibility = root % 5 == 0 ? 1U : 0U; }
    if (scenario == 2) { value.Visibility = 0; }
    if (scenario == 4) { value.Visibility = 2; }
    if (value.Visibility == 0) { continue; }
    expanded += value.Count;
    for (uint32_t at = 0; at < value.Count; at += kChunk) {
      expected.push_back({.Root = root, .First = at});
    }
  }
  return {.Entries = std::move(expected), .Expanded = expanded};
}

bool Run(Device &device,
         const ComputePipeline &reset,
         const ComputePipeline &expand,
         bool omitReset) {
  constexpr uint32_t copies = 8;
  std::vector<Root> roots;
  roots.reserve(kCounts.size() * copies);
  uint32_t logical = 0;
  uint32_t maximumChunks = 0;
  for (uint32_t copy = 0; copy < copies; ++copy) {
    for (uint32_t at = 0; at < kCounts.size(); ++at) {
      roots.push_back({.First = at, .Count = kCounts[at], .Logical = logical, .Visibility = 1});
      logical += kCounts[at];
      maximumChunks += (kCounts[at] + kChunk - 1) / kChunk;
    }
  }
  auto commands = device.Begin();
  if (!commands) { return false; }
  const std::array<uint32_t, 4> stale{123, 456, 789, 111};
  StorageBuffers storage;
  storage.State = Upload(device, **commands, std::as_bytes(std::span(stale)));
  const std::vector<Chunk> emptyQueue(maximumChunks + 2,
                                      {.Root = 0xdeadbeefu, .First = 0xdeadbeefu});
  storage.Queue = Upload(device, **commands, std::as_bytes(std::span(emptyQueue)));
  if (!storage.State || !storage.Queue) { return false; }
  auto ready = device.Submit(std::move(*commands));
  if (!ready || !device.Wait(*ready, std::chrono::seconds(5))) { return false; }
  for (unsigned scenario = 0; scenario < 5; ++scenario) {
    auto reference = Enumerate(scenario, roots, maximumChunks);
    auto &expected = reference.Entries;
    const auto expanded = reference.Expanded;
    View view{.Roots = static_cast<uint32_t>(roots.size()),
              .Capacity = maximumChunks,
              .ChunkSize = kChunk,
              .Unused = 0};
    if (scenario == 3) { view.Capacity = maximumChunks - 1; }
    const auto bytes =
        Execute(device, {.Reset = reset, .Expand = expand}, storage, roots, view, omitReset);
    if (bytes.empty()) { return false; }
    std::array<uint32_t, 4> counts{};
    std::memcpy(counts.data(), bytes.data(), sizeof(counts));
    std::vector<Chunk> chunks(emptyQueue.size());
    std::memcpy(chunks.data(), bytes.data() + sizeof(counts), chunks.size() * sizeof(Chunk));
    CHECK(counts[0] == expected.size() && counts[2] == expanded && counts[3] == roots.size(),
          "root-derived counters exactly match the independent CPU enumeration");
    CHECK((counts[1] != 0) == (scenario == 3),
          "overflow is explicit, never a valid truncated worklist");
    const Chunk sentinel{.Root = 0xdeadbeefu, .First = 0xdeadbeefu};
    CHECK(chunks[chunks.size() - 1] == sentinel && chunks[chunks.size() - 2] == sentinel,
          "queue writes never cross their physically allocated maximum");
    if (scenario != 3 && counts[0] == expected.size()) {
      chunks.resize(expected.size());
      const auto key = [](const Chunk &value) { return std::pair{value.Root, value.First}; };
      std::ranges::sort(chunks, {}, key);
      std::ranges::sort(expected, {}, key);
      CHECK(chunks == expected, "atomic queue order preserves every selected root/chunk identity");
    }
    std::println("WORK_QUEUE scenario={} roots={} full_jobs={} chunks={} expanded={} overflow={} "
                 "root_bytes={} queue_capacity_bytes={}",
                 scenario,
                 roots.size(),
                 logical,
                 counts[0],
                 counts[2],
                 counts[1],
                 roots.size() * sizeof(Root),
                 emptyQueue.size() * sizeof(Chunk));
  }
  return true;
}
}

int main(int argc, char **argv) {
  if (argc != 3 && argc != 4) { return 2; }
  auto device = CreateDevice();
  CHECK(device.has_value(), "actual native GPU is available");
  if (!device) { return Report(); }
  std::println("DEVICE {}", (*device)->Limits().Name);
  const auto resetWords = Spirv(argv[1]);
  const auto expandWords = Spirv(argv[2]);
  auto reset = (*device)->CreateComputePipeline(resetWords);
  auto expand = (*device)->CreateComputePipeline(expandWords);
  CHECK(reset && expand, "both actual native queue kernels compile from their SPIR-V");
  if (!reset || !expand) { return Report(); }
  CHECK(Run(**device, **reset, **expand, argc == 4),
        "bounded queue experiment reaches a terminal state");
  return Report();
}
