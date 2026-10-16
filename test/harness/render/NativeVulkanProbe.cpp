#include "src/render/device/native/Device.h"
#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <ios>
#include <limits>
#include <memory>
#include <print>
#include <span>
#include <string>
#include <thread>
#include <type_traits>
#include <utility>
#include <vector>

namespace {

using namespace outshine::Render::Native;

std::size_t gChecks = 0;

void Check(bool valid, const char *message) {
  ++gChecks;
  if (!valid) {
    std::println(stderr, "native Vulkan probe: {}", message);
    std::_Exit(EXIT_FAILURE);
  }
}

template <typename Value> Value Take(Result<Value> result) {
  if (!result) {
    std::println(stderr, "native Vulkan probe: {}", result.error().Message);
    std::_Exit(EXIT_FAILURE);
  }
  if constexpr (!std::is_void_v<Value>) { return std::move(*result); }
}

std::vector<uint32_t> ReadShader(const char *path) {
  std::ifstream file(path, std::ios::binary | std::ios::ate);
  Check(file.good(), "shader can be opened");
  const auto size = file.tellg();
  Check(size > 0 && size % sizeof(uint32_t) == 0, "shader has complete words");
  std::vector<uint32_t> words(static_cast<std::size_t>(size) / sizeof(uint32_t));
  file.seekg(0);
  file.read(reinterpret_cast<char *>(words.data()), static_cast<std::streamsize>(size));
  Check(file.good(), "shader can be read completely");
  return words;
}

struct alignas(16) Job {
  uint32_t Unused = 0;
  uint32_t Batch = 0;
  uint32_t First = 0;
  uint32_t Count = 0;
};

static_assert(sizeof(Job) == 16 && alignof(Job) == 16);
static_assert(offsetof(Job, Batch) == 4 && offsetof(Job, First) == 8 && offsetof(Job, Count) == 12);

struct Inputs {
  std::vector<Job> Jobs;
  std::vector<uint32_t> Source;
  std::vector<uint32_t> Kept;
  std::vector<uint32_t> Batches;
  std::vector<uint32_t> Arguments;
  std::vector<uint32_t> ExpectedSlots;
  std::vector<uint32_t> ExpectedArguments;
  std::vector<uint32_t> ExpectedIndices;
};

Inputs MakeInputs() {
  constexpr uint32_t jobs = 701;
  constexpr uint32_t split = 300;
  constexpr uint32_t longEvery = 61;
  constexpr uint32_t longJob = 137;
  constexpr uint32_t countVariants = 7;
  constexpr uint32_t rejectEvery = 5;
  constexpr uint32_t sourceStride = 1000;
  constexpr std::size_t argumentsPerBatch = 5;
  Inputs input;
  input.Batches = {0, split, split, 0, split, jobs - split};
  input.Arguments.resize(argumentsPerBatch * 3, 0);
  input.ExpectedSlots.resize(jobs, std::numeric_limits<uint32_t>::max());
  uint32_t batchCount = 0;
  for (uint32_t at = 0; at < jobs; ++at) {
    if (at == split) {
      input.Arguments[2] = 0;
      input.Arguments[argumentsPerBatch + 2] = batchCount;
      input.Arguments[2 * argumentsPerBatch + 2] = batchCount;
      batchCount = 0;
    }
    const auto count = at % longEvery == 0 ? longJob : at % countVariants + 1;
    input.Jobs.push_back({.Unused = 0,
                          .Batch = at < split ? 0u : 2u,
                          .First = static_cast<uint32_t>(input.Source.size()),
                          .Count = count});
    const bool kept = at % rejectEvery != 0;
    input.Kept.push_back(kept ? count : 0);
    for (uint32_t index = 0; index < count; ++index) {
      const auto value = at * sourceStride + index;
      input.Source.push_back(value);
      if (kept) { input.ExpectedIndices.push_back(value); }
    }
    if (kept) {
      input.ExpectedSlots[at] = batchCount;
      batchCount += count;
    }
  }
  for (std::size_t batch = 0; batch < 3; ++batch) {
    input.Arguments[argumentsPerBatch * batch + 1] = 1;
  }
  input.ExpectedArguments = input.Arguments;
  input.ExpectedArguments[0] = input.Arguments[argumentsPerBatch + 2];
  input.ExpectedArguments[argumentsPerBatch] = 0;
  input.ExpectedArguments[2 * argumentsPerBatch] = batchCount;
  return input;
}

template <typename Value> std::span<const std::byte> Bytes(const std::vector<Value> &values) {
  return std::as_bytes(std::span(values));
}

std::unique_ptr<Buffer> Upload(Device &device,
                               Commands &commands,
                               std::span<const std::byte> bytes,
                               std::vector<std::unique_ptr<Buffer>> &staging) {
  auto upload = Take(device.CreateBuffer(
      {.Bytes = bytes.size(), .Use = BufferUse::CopySource, .Placement = Memory::Upload}));
  auto target = Take(device.CreateBuffer(
      {.Bytes = bytes.size(),
       .Use = BufferUse::Storage | BufferUse::CopySource | BufferUse::CopyDestination,
       .Placement = Memory::Device}));
  const auto mapped = Take(device.MapWrite(*upload, {.Offset = 0, .Bytes = bytes.size()}));
  std::memcpy(mapped.data(), bytes.data(), bytes.size());
  Take(device.Unmap(*upload));
  Take(commands.Copy(
      *upload, *target, {.SourceOffset = 0, .DestinationOffset = 0, .Bytes = bytes.size()}));
  const auto busy = device.MapWrite(*upload, {.Offset = 0, .Bytes = bytes.size()});
  Check(!busy && busy.error().Kind == ErrorKind::Busy, "recorded upload cannot be overwritten");
  staging.push_back(std::move(upload));
  return target;
}

Binding Bind(uint32_t set, uint32_t slot, const Buffer &buffer) {
  return {.Set = set,
          .Slot = slot,
          .Value = StorageBuffer{.Data = &buffer,
                                 .Range = {.Offset = 0, .Bytes = buffer.Description().Bytes}}};
}

struct Outputs {
  std::unique_ptr<Buffer> Slots;
  std::unique_ptr<Buffer> Arguments;
  std::unique_ptr<Buffer> Indices;
};

Outputs Record(Device &device,
               Commands &commands,
               const Inputs &input,
               ComputePipeline &scan,
               ComputePipeline &compact,
               std::vector<std::unique_ptr<Buffer>> &staging) {
  auto kept = Upload(device, commands, Bytes(input.Kept), staging);
  auto batches = Upload(device, commands, Bytes(input.Batches), staging);
  auto jobs = Upload(device, commands, Bytes(input.Jobs), staging);
  auto source = Upload(device, commands, Bytes(input.Source), staging);
  auto arguments = Upload(device, commands, Bytes(input.Arguments), staging);
  const std::vector<uint32_t> zeroSlots(input.Kept.size(), 0);
  const std::vector<uint32_t> zeroIndices(input.ExpectedIndices.size(), 0);
  auto slots = Upload(device, commands, Bytes(zeroSlots), staging);
  auto indices = Upload(device, commands, Bytes(zeroIndices), staging);
  const std::array scanBindings = {
      Bind(0, 0, *kept), Bind(0, 1, *batches), Bind(1, 0, *slots), Bind(1, 1, *arguments)};
  Take(commands.Dispatch(scan, scanBindings, {.X = 3, .Y = 1, .Z = 1}));
  constexpr std::size_t viewBytes = 256;
  constexpr std::size_t jobsOffset = 112;
  std::array<std::byte, viewBytes> view{};
  const auto jobCount = static_cast<uint32_t>(input.Jobs.size());
  std::memcpy(view.data() + jobsOffset, &jobCount, sizeof(jobCount));
  const std::array compactBindings = {
      Bind(0, 0, *jobs),
      Bind(0, 1, *source),
      Bind(0, 2, *slots),
      Bind(0, 3, *arguments),
      Bind(1, 0, *indices),
      Binding{.Set = 2, .Slot = 0, .Value = Uniform{.Bytes = view}}};
  Take(commands.Dispatch(compact, compactBindings, {.X = jobCount, .Y = 1, .Z = 1}));
  view.fill(std::byte{});
  return {
      .Slots = std::move(slots), .Arguments = std::move(arguments), .Indices = std::move(indices)};
}

void Compare(Device &device, Buffer &readback, const std::vector<uint32_t> &expected) {
  const auto range = ByteRange{.Offset = 0, .Bytes = expected.size() * sizeof(uint32_t)};
  const auto mapped = Take(device.MapRead(readback, range));
  for (std::size_t at = 0; at < expected.size(); ++at) {
    uint32_t value = 0;
    std::memcpy(&value, mapped.data() + at * sizeof(uint32_t), sizeof(value));
    Check(value == expected[at], "native compute output equals independent serial oracle");
  }
  Take(device.Unmap(readback));
}

std::unique_ptr<Buffer> Readback(Device &device, Commands &commands, const Buffer &source) {
  const auto bytes = source.Description().Bytes;
  auto target = Take(device.CreateBuffer(
      {.Bytes = bytes, .Use = BufferUse::CopyDestination, .Placement = Memory::Readback}));
  Take(commands.Copy(source, *target, {.SourceOffset = 0, .DestinationOffset = 0, .Bytes = bytes}));
  const auto early = device.MapRead(*target, {.Offset = 0, .Bytes = bytes});
  Check(!early && early.error().Kind == ErrorKind::Busy,
        "readback is unavailable before submission");
  return target;
}

void CheckCancel(Device &device) {
  bool rejectedThread = false;
  std::thread other([&] {
    const auto result = device.Begin();
    rejectedThread = !result && result.error().Kind == ErrorKind::Invalid;
  });
  other.join();
  Check(rejectedThread, "GPU operations reject a different owner thread");
  std::vector<std::unique_ptr<Commands>> reservations;
  constexpr std::size_t reservationGuard = 32;
  for (;;) {
    auto next = device.Begin();
    if (!next) {
      Check(next.error().Kind == ErrorKind::Busy, "command acquisition has bounded backpressure");
      break;
    }
    reservations.push_back(std::move(*next));
    Check(reservations.size() < reservationGuard,
          "unsubmitted commands cannot grow without a bound");
  }
  reservations.clear();
  auto available = Take(device.Begin());
  available.reset();
  constexpr uint64_t bytes = 64;
  auto source = Take(device.CreateBuffer(
      {.Bytes = bytes, .Use = BufferUse::CopySource, .Placement = Memory::Upload}));
  auto target = Take(device.CreateBuffer(
      {.Bytes = bytes, .Use = BufferUse::CopyDestination, .Placement = Memory::Device}));
  auto commands = Take(device.Begin());
  Take(commands->Copy(
      *source, *target, {.SourceOffset = 0, .DestinationOffset = 0, .Bytes = bytes}));
  const auto busy = device.MapWrite(*source, {.Offset = 0, .Bytes = bytes});
  Check(!busy && busy.error().Kind == ErrorKind::Busy, "unsubmitted resource is retained");
  commands.reset();
  const auto writable = Take(device.MapWrite(*source, {.Offset = 0, .Bytes = bytes}));
  Check(writable.size() == bytes, "cancel releases unsubmitted resource use");
  Take(device.Unmap(*source));
  const auto invalid =
      device.MapWrite(*source, {.Offset = std::numeric_limits<uint64_t>::max(), .Bytes = bytes});
  Check(!invalid && invalid.error().Kind == ErrorKind::Invalid, "overflowing range is rejected");
}

}

int main(int argc, char **argv) {
  if (argc != 3 && argc != 4) {
    std::println(stderr, "usage: native_vulkan scan.spv compact.spv [--validation]");
    return EXIT_FAILURE;
  }
  const bool validation = argc == 4 && std::string(argv[3]) == "--validation";
  auto device = Take(CreateDevice({.Validation = validation}));
  Check(device->Limits().Name.find("RTX 3060") != std::string::npos,
        "hardware proof uses the RTX 3060");
  CheckCancel(*device);
  const auto input = MakeInputs();
  auto scan = Take(device->CreateComputePipeline(ReadShader(argv[1])));
  auto compact = Take(device->CreateComputePipeline(ReadShader(argv[2])));
  Check(scan->Layout().LocalSize[0] == 256, "scan workgroup reflected exactly");
  Check(compact->Layout().LocalSize[0] == 128, "compact workgroup reflected exactly");
  auto commands = Take(device->Begin());
  const auto frame = device->AcquirePresentFrame();
  Check(!frame && frame.error().Kind == ErrorKind::Unsupported,
        "a headless device does not invent a present frame");
  const auto presented = commands->Present(nullptr);
  Check(!presented && presented.error().Kind == ErrorKind::Unsupported,
        "a headless recording cannot silently accept presentation");
  std::vector<std::unique_ptr<Buffer>> staging;
  auto output = Record(*device, *commands, input, *scan, *compact, staging);
  auto slots = Readback(*device, *commands, *output.Slots);
  auto arguments = Readback(*device, *commands, *output.Arguments);
  auto indices = Readback(*device, *commands, *output.Indices);
  staging.clear();
  output = {};
  scan.reset();
  compact.reset();
  const auto completion = Take(device->Submit(std::move(commands)));
  Check(Take(device->Wait(completion, std::chrono::seconds(5))) == CompletionStatus::Ready,
        "bounded GPU completion succeeds");
  Compare(*device, *slots, input.ExpectedSlots);
  Compare(*device, *arguments, input.ExpectedArguments);
  Compare(*device, *indices, input.ExpectedIndices);
  Check(Take(device->Poll(completion)) == CompletionStatus::Ready, "completed token stays ready");
  const auto badToken =
      device->Poll({.Device = completion.Device + 1, .Serial = completion.Serial});
  Check(!badToken && badToken.error().Kind == ErrorKind::Invalid,
        "foreign completion token is rejected");
  const auto negativeWait = device->Wait(completion, std::chrono::nanoseconds(-1));
  Check(!negativeWait && negativeWait.error().Kind == ErrorKind::Invalid,
        "negative wait is rejected");
  std::println(
      R"({{"device":"{}","checks":{},"jobs":{},"indices":{},"validation":{},"depth_clamp":{},"gpu_timing":"unavailable","places":"unproven"}})",
      device->Limits().Name,
      gChecks,
      input.Jobs.size(),
      input.ExpectedIndices.size(),
      validation,
      device->Limits().DepthClamp);
  return EXIT_SUCCESS;
}
