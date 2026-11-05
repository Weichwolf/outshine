#include "Check.h"
#include "test/harness/render/NativeGpuCheck.h"
#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <SDL3/SDL_stdinc.h>
#include <cstdio>
#include <utility>
#include <string_view>
#include <ios>
#include <filesystem>
#include <fstream>
#include <limits>
#include <memory>
#include <numeric>
#include <print>
#include <span>
#include <string>
#include <vector>

namespace {
using namespace outshine::Render::Native;
using namespace outshine::Test;
using namespace outshine::Test::Gpu;

std::vector<uint32_t> Read(const std::filesystem::path &file) {
  std::ifstream input(file, std::ios::binary | std::ios::ate);
  const auto size = input ? static_cast<std::streamoff>(input.tellg()) : -1;
  CHECK(size > 0 && size % 4 == 0 &&
            std::cmp_less_equal(size, std::numeric_limits<uint32_t>::max()),
        "a complete bounded word stream exists");
  if (size <= 0 || size % 4 != 0 || std::cmp_greater(size, std::numeric_limits<uint32_t>::max())) {
    Failed();
  }
  std::vector<uint32_t> result(static_cast<size_t>(size) / 4);
  input.seekg(0);
  input.read(reinterpret_cast<char *>(result.data()), size);
  CHECK(input, "the complete stream is read");
  if (!input) { Failed(); }
  return result;
}

static_assert(sizeof(std::array<uint32_t, 4>) == 16);

struct Fixture {
  std::array<std::unique_ptr<Buffer>, 5> Buffers;
  std::array<uint32_t, 8> View{};
  std::vector<uint32_t> ExpectedArguments, ExpectedSlots, Metadata;
  std::vector<double> Times;
};

Fixture Prepare(Device &device, const std::filesystem::path &path) {
  std::array inputs{Read(path / "roots.bin"),
                    Read(path / "visible.bin"),
                    Read(path / "kept.bin"),
                    std::vector<uint32_t>{},
                    Read(path / "arguments.bin")};
  const size_t roots = inputs[0].size() / 12;
  const size_t logical = inputs[2].size();
  const size_t batches = inputs[4].size() / 5;
  CHECK(inputs[0].size() % 12 == 0 && inputs[1].size() == roots && inputs[4].size() % 5 == 0 &&
            roots <= std::numeric_limits<uint32_t>::max() &&
            logical <= std::numeric_limits<uint32_t>::max(),
        "root and argument shapes fit");
  if (inputs[0].size() % 12 != 0 || inputs[1].size() != roots || inputs[4].size() % 5 != 0) {
    Failed();
  }
  inputs[3].assign(logical + batches, 0xffffffffu);
  std::fill(inputs[3].begin() + static_cast<std::ptrdiff_t>(logical), inputs[3].end(), 2u);
  Fixture result;
  result.ExpectedArguments = inputs[4];
  result.ExpectedSlots.assign(logical, 0xffffffffu);
  result.Metadata.resize(logical * 2);
  std::vector<uint64_t> prefix(batches);
  size_t nextLogical = 0;
  size_t previousBatch = 0;
  for (size_t row = 0; row < roots; ++row) {
    const size_t from = inputs[0][row * 12 + 6];
    const size_t count = inputs[0][row * 12 + 5];
    const size_t batch = inputs[0][row * 12 + 7];
    CHECK(from == nextLogical && batch >= previousBatch && from <= logical &&
              count <= logical - from && batch < batches,
          "logical and batch ranges fit");
    if (from != nextLogical || batch < previousBatch || from > logical || count > logical - from ||
        batch >= batches) {
      Failed();
    }
    nextLogical += count;
    previousBatch = batch;
    for (size_t one = 0; one < count; ++one) {
      result.Metadata[(from + one) * 2] = static_cast<uint32_t>(row);
      result.Metadata[(from + one) * 2 + 1] = static_cast<uint32_t>(batch);
    }
    if (inputs[1][row] == 0) { continue; }
    for (size_t one = 0; one < count; ++one) {
      const uint32_t indices = inputs[2][from + one];
      if (indices == 0) { continue; }
      CHECK(prefix[batch] <= std::numeric_limits<uint32_t>::max(), "prefix fits uint32");
      result.ExpectedSlots[from + one] = static_cast<uint32_t>(prefix[batch]);
      prefix[batch] += indices;
    }
  }
  CHECK(nextLogical == logical, "logical roots cover the complete selection array");
  for (size_t batch = 0; batch < batches; ++batch) {
    CHECK(prefix[batch] == inputs[4][batch * 5], "CPU prefixes match captured draw counts");
    inputs[4][batch * 5] = 0;
  }
  result.View[0] = static_cast<uint32_t>(roots);
  result.View[5] = std::min(256u, device.Limits().MaxWorkgroupCount[0]);
  result.View[6] = static_cast<uint32_t>(logical);
  auto commands = Require(device.Begin());
  for (size_t at = 0; at < inputs.size(); ++at) {
    result.Buffers[at] = Upload(device, *commands, inputs[at]);
  }
  const auto done = Require(device.Submit(std::move(commands)));
  CHECK(Require(device.Wait(done, std::chrono::seconds(10))) == CompletionStatus::Ready,
        "input uploads complete");
  return result;
}

struct ScanLevel {
  std::unique_ptr<Buffer> Prefix, Tail;
  uint32_t Count = 0;
};

struct ParallelScan {
  std::unique_ptr<Buffer> Metadata;
  std::vector<ScanLevel> Levels;
  std::array<std::unique_ptr<ComputePipeline>, 4> Programs;
};

ParallelScan
PrepareParallel(Device &device, const Fixture &fixture, const std::filesystem::path &directory) {
  ParallelScan result;
  constexpr std::array names{"prefix-first.spv", "prefix-block.spv", "carry.spv", "finish.spv"};
  for (size_t at = 0; at < names.size(); ++at) {
    result.Programs[at] =
        Require(device.CreateComputePipeline(Spirv((directory / names[at]).c_str())));
  }
  auto commands = Require(device.Begin());
  result.Metadata = Upload(device, *commands, fixture.Metadata);
  const auto done = Require(device.Submit(std::move(commands)));
  CHECK(Require(device.Wait(done, std::chrono::seconds(10))) == CompletionStatus::Ready,
        "cluster metadata upload completes");
  uint32_t count = fixture.View[6];
  while (true) {
    const uint32_t blocks = count / 256u + uint32_t{count % 256u != 0};
    result.Levels.push_back({.Prefix = Require(device.CreateBuffer(
                                 {.Bytes = uint64_t{count} * 8, .Use = BufferUse::Storage})),
                             .Tail = Require(device.CreateBuffer(
                                 {.Bytes = uint64_t{blocks} * 8, .Use = BufferUse::Storage})),
                             .Count = count});
    if (blocks == 1) { break; }
    count = blocks;
  }
  return result;
}

Binding ScanView(uint32_t count, uint32_t width, std::array<uint32_t, 4> &words) {
  words = {count, width, 0, 0};
  return {.Set = 2, .Slot = 0, .Value = Uniform{std::as_bytes(std::span(words))}};
}

void Parallel(const Device &device,
              Commands &commands,
              const Fixture &fixture,
              const ParallelScan &scan) {
  const uint32_t width = std::min(256u, device.Limits().MaxWorkgroupCount[0]);
  const auto grid = [width](uint32_t count) {
    const uint32_t blocks = count / 256u + uint32_t{count % 256u != 0};
    return Workgroups{.X = std::min(width, blocks),
                      .Y = blocks / width + uint32_t{blocks % width != 0}};
  };
  for (size_t at = 0; at < scan.Levels.size(); ++at) {
    const auto &level = scan.Levels[at];
    const auto &source = at == 0 ? scan.Metadata : scan.Levels[at - 1].Tail;
    std::array<uint32_t, 4> view{};
    std::vector bindings{Storage(0, 0, *source),
                         Storage(1, 0, *level.Prefix),
                         Storage(1, 1, *level.Tail),
                         ScanView(level.Count, width, view)};
    if (at == 0) {
      bindings.push_back(Storage(0, 1, *fixture.Buffers[2]));
      bindings.push_back(Storage(0, 2, *fixture.Buffers[1]));
    }
    Require(commands.Dispatch(*scan.Programs[at == 0 ? 0 : 1], bindings, grid(level.Count)));
  }
  for (size_t at = scan.Levels.size() - 1; at > 1; --at) {
    const auto &level = scan.Levels[at - 1];
    std::array<uint32_t, 4> view{};
    const std::array bindings{Storage(0, 0, *scan.Levels[at].Prefix),
                              Storage(1, 0, *level.Prefix),
                              ScanView(level.Count, width, view)};
    Require(commands.Dispatch(*scan.Programs[2], bindings, grid(level.Count)));
  }
  const auto &first = scan.Levels.front();
  const auto &parent = scan.Levels.size() > 1 ? scan.Levels[1].Prefix : first.Tail;
  std::array<uint32_t, 4> view{};
  const std::array bindings{Storage(0, 0, *first.Prefix),
                            Storage(0, 1, *parent),
                            Storage(1, 0, *fixture.Buffers[3]),
                            Storage(1, 1, *fixture.Buffers[4]),
                            ScanView(first.Count, width, view)};
  Require(commands.Dispatch(*scan.Programs[3], bindings, grid(first.Count)));
}

double Run(Device &device,
           const ComputePipeline &pipeline,
           const Fixture &fixture,
           const ParallelScan *parallel = nullptr) {
  std::array bindings{
      Storage(0, 0, *fixture.Buffers[0]),
      Storage(0, 1, *fixture.Buffers[1]),
      Storage(0, 2, *fixture.Buffers[2]),
      Storage(1, 0, *fixture.Buffers[3]),
      Storage(1, 1, *fixture.Buffers[4]),
      Binding{.Set = 2, .Slot = 0, .Value = Uniform{std::as_bytes(std::span(fixture.View))}}};
  auto commands = Require(device.Begin());
  const uint32_t width = fixture.View[5];
  if (parallel != nullptr) {
    Parallel(device, *commands, fixture, *parallel);
  } else {
    Require(commands->Dispatch(
        pipeline,
        bindings,
        {.X = std::min(width, fixture.View[0]), .Y = (fixture.View[0] + width - 1) / width}));
  }
  const auto done = Require(device.Submit(std::move(commands)));
  CHECK(Require(device.Wait(done, std::chrono::seconds(10))) == CompletionStatus::Ready,
        "the native scan terminates");
  const auto timing = Require(device.SubmissionGpuTime(done));
  CHECK(timing.Status == CompletionStatus::Ready && std::isfinite(timing.ElapsedNanoseconds) &&
            timing.ElapsedNanoseconds > 0,
        "native GPU submission timing is available");
  return timing.ElapsedNanoseconds / 1e6;
}

std::vector<uint32_t> Download(Device &device, const Buffer &source, size_t words) {
  auto buffer = Require(device.CreateBuffer(
      {.Bytes = words * 4, .Use = BufferUse::CopyDestination, .Placement = Memory::Readback}));
  auto commands = Require(device.Begin());
  Require(commands->Copy(source, *buffer, {.Bytes = words * 4}));
  const auto done = Require(device.Submit(std::move(commands)));
  CHECK(Require(device.Wait(done, std::chrono::seconds(10))) == CompletionStatus::Ready,
        "the completed scan is readable");
  const auto bytes = Require(device.MapRead(*buffer, {.Bytes = words * 4}));
  std::vector<uint32_t> result(words);
  std::memcpy(result.data(), bytes.data(), bytes.size());
  Require(device.Unmap(*buffer));
  return result;
}

void Verify(Device &device, const Fixture &fixture, const char *name) {
  CHECK(Download(device, *fixture.Buffers[4], fixture.ExpectedArguments.size()) ==
            fixture.ExpectedArguments,
        "the real GPU scan preserves all captured argument fields");
  CHECK(Download(device, *fixture.Buffers[3], fixture.ExpectedSlots.size()) ==
            fixture.ExpectedSlots,
        "every GPU cluster prefix matches the independent CPU scan");
  const double sum = std::accumulate(fixture.Times.begin(), fixture.Times.end(), 0.0);
  std::println("{} roots={} batches={} clusters={} gpu_mean_ms={:.6f} gpu_max_ms={:.6f}",
               name,
               fixture.View[0],
               fixture.ExpectedArguments.size() / 5,
               fixture.View[6],
               sum / static_cast<double>(fixture.Times.size()),
               *std::ranges::max_element(fixture.Times));
}

void ReplaceCounts(Device &device, Fixture &fixture, std::span<const uint32_t> words) {
  auto commands = Require(device.Begin());
  const auto replacement = Upload(device, *commands, words);
  Require(commands->Copy(*replacement, *fixture.Buffers[2], {.Bytes = words.size_bytes()}));
  const auto done = Require(device.Submit(std::move(commands)));
  CHECK(Require(device.Wait(done, std::chrono::seconds(10))) == CompletionStatus::Ready,
        "the next frame's selection replaces the preceding GPU input");
}

void VerifySelectionChange(Device &device,
                           const ComputePipeline &pipeline,
                           Fixture &fixture,
                           const ParallelScan &scan,
                           std::span<const uint32_t> original) {
  const auto arguments = fixture.ExpectedArguments;
  const auto slots = fixture.ExpectedSlots;
  ReplaceCounts(device, fixture, std::vector<uint32_t>(original.size(), 0));
  std::ranges::fill(fixture.ExpectedSlots, 0xffffffffu);
  for (size_t at = 0; at < arguments.size(); at += 5) { fixture.ExpectedArguments[at] = 0; }
  fixture.Times = {Run(device, pipeline, fixture, &scan)};
  Verify(device, fixture, "zero_after_visible");
  ReplaceCounts(device, fixture, original);
  fixture.ExpectedArguments = arguments;
  fixture.ExpectedSlots = slots;
  fixture.Times = {Run(device, pipeline, fixture, &scan)};
  Verify(device, fixture, "visible_after_zero");
  fixture.Times.clear();
}
}

int main(int argc, const char *argv[]) {
  if (argc != 3 && argc != 4) {
    std::println(stderr,
                 "GeometryGroupScanProbe <resident-snapshot> <grouped-plan> [parallel-shaders]");
    return 2;
  }
  const char *validation = SDL_GetEnvironmentVariable(SDL_GetEnvironment(), "MTL_DEBUG_LAYER");
  auto device = Require(
      CreateDevice({.Validation = validation != nullptr && std::string_view(validation) == "1",
                    .GpuTiming = GpuTimingMode::Submission}));
  const auto code = Shader("geometryRootScan.comp.spv");
  const auto pipeline = Require(device->CreateComputePipeline(code));
  std::array fixtures{Prepare(*device, argv[1]), Prepare(*device, argv[2])};
  std::unique_ptr<ParallelScan> parallel;
  std::vector<double> parallelTimes;
  if (argc == 4) {
    parallel = std::make_unique<ParallelScan>(PrepareParallel(*device, fixtures[1], argv[3]));
    const double ms = Run(*device, *pipeline, fixtures[1], parallel.get());
    fixtures[1].Times.push_back(ms);
    Verify(*device, fixtures[1], "parallel_initial_verification");
    fixtures[1].Times.clear();
    VerifySelectionChange(*device,
                          *pipeline,
                          fixtures[1],
                          *parallel,
                          Read(std::filesystem::path(argv[2]) / "kept.bin"));
  }
  const size_t modes = parallel ? 3u : 2u;
  for (size_t round = 0; round < 26; ++round) {
    for (size_t at = 0; at < modes; ++at) {
      const size_t mode = (at + round) % modes;
      auto &fixture = fixtures[mode == 2 ? 1 : mode];
      const double ms = Run(*device, *pipeline, fixture, mode == 2 ? parallel.get() : nullptr);
      if (round < 2) { continue; }
      (mode == 2 ? parallelTimes : fixture.Times).push_back(ms);
    }
  }
  Verify(*device, fixtures[0], "source");
  Verify(*device, fixtures[1], "grouped");
  if (parallel) {
    fixtures[1].Times = std::move(parallelTimes);
    Verify(*device, fixtures[1], "grouped_parallel");
  }
  return Report();
}
