#include "NativeGpuCheck.h"

#include <algorithm>
#include <array>
#include <bit>
#include <chrono>
#include <cstring>
#include <fstream>
#include <limits>
#include <print>
#include <span>
#include <thread>

namespace {
using namespace outshine::Render::Native;
using namespace outshine::Test;
using namespace outshine::Test::Gpu;

struct Data {
  std::vector<uint32_t> Kept, Batches, Jobs, Source, Slots, Args, Draw;
};

Data Inputs() {
  constexpr uint32_t sentinel = 0xdeadbeefu;
  constexpr uint32_t sourceBias = 17;
  Data data;
  uint32_t first = 0;
  for (const auto count : {301u, 1u, 399u}) {
    const auto batch = static_cast<uint32_t>(data.Batches.size() / 2);
    data.Batches.insert(data.Batches.end(), {first, count});
    const auto base = static_cast<uint32_t>(data.Draw.size());
    uint32_t kept = 0;
    uint32_t possible = 0;
    for (uint32_t i = 0; i < count; ++i) {
      const auto job = first + i;
      const auto indices = 3 * (job % 3 + 1);
      const auto source = static_cast<uint32_t>(data.Source.size());
      const bool visible = job % 5 != 0;
      data.Kept.push_back(visible ? indices : 0);
      data.Slots.push_back(visible ? kept : std::numeric_limits<uint32_t>::max());
      data.Jobs.insert(data.Jobs.end(), {0, batch, source, indices});
      for (uint32_t index = 0; index < indices; ++index) {
        data.Source.push_back(13 * (source + index) + sourceBias);
      }
      if (visible) { kept += indices; }
      possible += indices;
    }
    data.Args.insert(data.Args.end(), {kept, 1, base, 0, 0});
    data.Draw.resize(base + possible + 4, sentinel);
    for (uint32_t i = first; i < first + count; ++i) {
      if (data.Kept[i] == 0) { continue; }
      std::copy_n(data.Source.begin() + data.Jobs[i * 4 + 2],
                  data.Kept[i],
                  data.Draw.begin() + base + data.Slots[i]);
    }
    first += count;
  }
  return data;
}

constexpr size_t kJobCountOffset = 112, kCullTrailingBytes = 140;

struct alignas(16) CullConstants {
  std::array<std::byte, kJobCountOffset> PlaneAndOrigin{};
  uint32_t Jobs = 701;
  std::array<std::byte, kCullTrailingBytes> Rest{};
};

static_assert(sizeof(CullConstants) == 256 && alignof(CullConstants) == 16 &&
              offsetof(CullConstants, Jobs) == kJobCountOffset);

void ScanCompact(Device &device, const char *scanPath, const char *compactPath) {
  auto scan = Require(device.CreateComputePipeline(Spirv(scanPath)));
  auto compact = Require(device.CreateComputePipeline(Spirv(compactPath)));
  const auto data = Inputs();
  auto commands = Require(device.Begin());
  auto kept = Upload(device, *commands, data.Kept);
  auto batches = Upload(device, *commands, data.Batches);
  auto jobs = Upload(device, *commands, data.Jobs);
  auto source = Upload(device, *commands, data.Source);
  const std::vector<uint32_t> emptySlots(data.Slots.size(), 0);
  auto slots = Upload(device, *commands, emptySlots);
  auto initialArgs = data.Args;
  for (size_t at = 0; at < initialArgs.size(); at += 5) { initialArgs[at] = 0; }
  auto args = Upload(device, *commands, initialArgs);
  const std::vector<uint32_t> untouched(data.Draw.size(), 0xdeadbeefu);
  auto draw = Upload(device, *commands, untouched);
  const std::array scanBindings{
      Storage(0, 0, *kept), Storage(0, 1, *batches), Storage(1, 0, *slots), Storage(1, 1, *args)};
  const auto overflowing =
      commands->Dispatch(*scan, scanBindings, {.X = std::numeric_limits<uint32_t>::max()});
  CHECK(!overflowing && overflowing.error().Kind == ErrorKind::Invalid,
        "dispatch grid multiplication cannot overflow shader coordinates");
  Require(commands->Dispatch(*scan, scanBindings, {.X = 3}));
  CullConstants constants;
  const std::array compactBindings{
      Storage(0, 0, *jobs),
      Storage(0, 1, *source),
      Storage(0, 2, *slots),
      Storage(0, 3, *args),
      Storage(1, 0, *draw),
      Binding{.Set = 2, .Slot = 0, .Value = Uniform{std::as_bytes(std::span(&constants, 1))}}};
  Require(commands->Dispatch(*compact, compactBindings, {.X = constants.Jobs}));
  constants.Jobs = 0;
  const auto bytes =
      slots->Description().Bytes + args->Description().Bytes + draw->Description().Bytes;
  auto readback = Require(device.CreateBuffer(
      {.Bytes = bytes, .Use = BufferUse::CopyDestination, .Placement = Memory::Readback}));
  uint64_t offset = 0;
  for (const auto *buffer : {slots.get(), args.get(), draw.get()}) {
    Require(commands->Copy(
        *buffer, *readback, {.DestinationOffset = offset, .Bytes = buffer->Description().Bytes}));
    offset += buffer->Description().Bytes;
  }
  kept.reset();
  batches.reset();
  jobs.reset();
  source.reset();
  slots.reset();
  args.reset();
  draw.reset();
  scan.reset();
  compact.reset();
  const auto token = Require(device.Submit(std::move(commands)));
  CHECK(Require(device.Wait(token, std::chrono::seconds(10))) == CompletionStatus::Ready,
        "native work completes within the explicit diagnostic budget");
  const auto mapped = Require(device.MapRead(*readback, {.Bytes = bytes}));
  std::vector<uint32_t> actual(mapped.size() / sizeof(uint32_t));
  std::memcpy(actual.data(), mapped.data(), mapped.size());
  size_t at = 0;
  for (const auto *expected : {&data.Slots, &data.Args, &data.Draw}) {
    for (const auto value : *expected) {
      CHECK(actual[at++] == value,
            "native scan/compact matches the independent packed-index reference");
    }
  }
  Require(device.Unmap(*readback));
}

void Boundaries(Device &device) {
  auto upload = Require(device.CreateBuffer(
      {.Bytes = 16, .Use = BufferUse::CopySource, .Placement = Memory::Upload}));
  auto target = Require(device.CreateBuffer({.Bytes = 16, .Use = BufferUse::CopyDestination}));
  auto commands = Require(device.Begin());
  Require(commands->Copy(*upload, *target, {.Bytes = 16}));
  const auto busy = device.MapWrite(*upload, {.Bytes = 16});
  CHECK(!busy && busy.error().Kind == ErrorKind::Busy,
        "unsubmitted recording reserves its uploads");
  commands.reset();
  Require(device.MapWrite(*upload, {.Bytes = 16}));
  Require(device.Unmap(*upload));
  const auto invalid =
      device.MapWrite(*upload, {.Offset = std::numeric_limits<uint64_t>::max(), .Bytes = 4});
  CHECK(!invalid && invalid.error().Kind == ErrorKind::Invalid,
        "overflowing host ranges are rejected");
  const auto foreign = device.Poll({.Device = std::numeric_limits<uint64_t>::max(), .Serial = 1});
  CHECK(!foreign && foreign.error().Kind == ErrorKind::Invalid,
        "foreign completion tokens are rejected");
  auto empty = Require(device.Begin());
  const auto token = Require(device.Submit(std::move(empty)));
  const auto negative = device.Wait(token, std::chrono::nanoseconds(-1));
  CHECK(!negative && negative.error().Kind == ErrorKind::Invalid,
        "negative wait budgets are rejected");
  CHECK(Require(device.Wait(token, std::chrono::seconds(10))) == CompletionStatus::Ready,
        "empty submissions are valid ordered completions");
}

void BufferLengths(Device &device, const char *path) {
  constexpr uint32_t sourceWords = 101;
  constexpr uint32_t rangeWords = 77;
  constexpr uint32_t outputWords = 73;
  constexpr uint32_t skippedWords = 3;
  constexpr uint32_t added = 23;
  constexpr uint32_t reverseAdd = 5;
  constexpr uint32_t finalAdd = 11;
  auto pipeline = Require(device.CreateComputePipeline(Spirv(path)));
  auto commands = Require(device.Begin());
  std::vector<uint32_t> input(sourceWords);
  for (uint32_t at = 0; at < sourceWords; ++at) { input[at] = 13 * at; }
  auto source = Upload(device, *commands, input);
  auto output = Upload(device, *commands, std::vector<uint32_t>(outputWords));
  std::array<uint32_t, 64> parameters{};
  parameters[0] = added;
  std::array bindings{
      Storage(0, 0, *source),
      Storage(1, 0, *output),
      Binding{.Set = 2, .Slot = 0, .Value = Uniform{std::as_bytes(std::span(parameters))}}};
  std::get<StorageBuffer>(bindings[0].Value).Range = {.Offset = skippedWords * sizeof(uint32_t),
                                                      .Bytes = rangeWords * sizeof(uint32_t)};
  Require(commands->Dispatch(*pipeline, bindings, {.X = 2}));
  parameters[0] = reverseAdd;
  std::array reversed{Storage(0, 0, *output), Storage(1, 0, *source), bindings[2]};
  std::get<StorageBuffer>(reversed[1].Value).Range = {.Offset = skippedWords * sizeof(uint32_t),
                                                      .Bytes = rangeWords * sizeof(uint32_t)};
  Require(commands->Dispatch(*pipeline, reversed, {.X = 2}));
  parameters[0] = finalAdd;
  Require(commands->Dispatch(*pipeline, bindings, {.X = 2}));
  parameters.fill(0);
  auto readback = Require(device.CreateBuffer({.Bytes = outputWords * sizeof(uint32_t),
                                               .Use = BufferUse::CopyDestination,
                                               .Placement = Memory::Readback}));
  Require(commands->Copy(*output, *readback, {.Bytes = outputWords * sizeof(uint32_t)}));
  source.reset();
  output.reset();
  pipeline.reset();
  const auto token = Require(device.Submit(std::move(commands)));
  CHECK(Require(device.Wait(token, std::chrono::seconds(10))) == CompletionStatus::Ready,
        "range-length kernel completes before host inspection");
  const auto mapped = Require(device.MapRead(*readback, {.Bytes = outputWords * sizeof(uint32_t)}));
  for (uint32_t at = 0; at < outputWords; ++at) {
    uint32_t value = 0;
    std::memcpy(&value, mapped.data() + at * sizeof(uint32_t), sizeof(value));
    CHECK(value == input[at + skippedWords] + added + rangeWords + reverseAdd + outputWords +
                       finalAdd + rangeWords,
          "MSL runtime array length uses the bound byte range, not the allocation size");
  }
  Require(device.Unmap(*readback));
}

void RecordingCapacity(Device &device) {
  std::vector<std::unique_ptr<Commands>> held;
  constexpr size_t probeLimit = 32;
  while (held.size() < probeLimit) {
    auto commands = device.Begin();
    if (!commands) {
      CHECK(commands.error().Kind == ErrorKind::Busy,
            "recording pressure reports recoverable backpressure");
      break;
    }
    held.push_back(std::move(*commands));
  }
  CHECK(!held.empty() && held.size() < probeLimit,
        "recording pressure is bounded before driver acquisition blocks");
  held.clear();
  const auto reusable = Require(device.Begin());
  CHECK(reusable, "cancelling recordings restores command capacity");
}

void ForeignThreads(Device &device) {
  auto source = Require(device.CreateBuffer(
      {.Bytes = 16, .Use = BufferUse::CopySource, .Placement = Memory::Upload}));
  auto destination = Require(device.CreateBuffer({.Bytes = 16, .Use = BufferUse::CopyDestination}));
  auto commands = Require(device.Begin());
  std::array<ErrorKind, 3> errors;
  errors.fill(ErrorKind::Driver);
  std::thread worker([&] {
    const auto recording = device.Begin();
    if (!recording) { errors[0] = recording.error().Kind; }
    const auto mapping = device.MapWrite(*source, {.Bytes = 16});
    if (!mapping) { errors[1] = mapping.error().Kind; }
    const auto copy = commands->Copy(*source, *destination, {.Bytes = 16});
    if (!copy) { errors[2] = copy.error().Kind; }
  });
  worker.join();
  for (const auto error : errors) {
    CHECK(error == ErrorKind::Invalid,
          "device and recording reject operations from a foreign thread");
  }
}

}

int main(int argc, char *const *argv) {
  if (argc != 4) { return 1; }
  auto device = Require(CreateDevice());
  Boundaries(*device);
  RecordingCapacity(*device);
  ForeignThreads(*device);
  ScanCompact(*device, argv[1], argv[2]);
  BufferLengths(*device, argv[3]);
  return Report();
}
