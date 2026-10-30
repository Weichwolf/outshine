#include "NativeGpuCheck.h"
#include "Check.h"
#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <fstream>
#include <ios>
#include <iosfwd>
#include <memory>
#include <print>
#include <span>
#include <vector>
#include <utility>

namespace {
using namespace outshine::Test;
using namespace outshine::Test::Gpu;

std::vector<uint32_t> Read(const char *path) {
  std::ifstream input(path, std::ios::binary | std::ios::ate);
  constexpr std::streamoff maximum = std::streamoff{16} * 1024 * 1024;
  const std::streamoff bytes = input ? input.tellg() : std::streampos(-1);
  CHECK(bytes > 0 && bytes <= maximum && bytes % sizeof(uint32_t) == 0,
        "input words have bounded aligned size");
  if (bytes <= 0 || bytes > maximum || bytes % sizeof(uint32_t) != 0) { Failed(); }
  std::vector<uint32_t> result(static_cast<size_t>(bytes) / sizeof(uint32_t));
  input.seekg(0);
  input.read(reinterpret_cast<char *>(result.data()), bytes);
  CHECK(input.good(), "input words completely read");
  return result;
}
}

int main(int argc, char **argv) {
  if (argc != 5) { return 2; }
  const auto data = Read(argv[2]);
  const auto words = Read(argv[3]);
  constexpr size_t stride = 24;
  CHECK(data.size() % stride == 0, "query input is a complete sequence of96B records");
  if (data.size() % stride != 0) { return 2; }
  auto device = Require(CreateDevice({.Validation = true}));
  auto pipeline = Require(device->CreateComputePipeline(Spirv(argv[1])));
  auto commands = Require(device->Begin());
  auto queries = Upload(*device, *commands, data);
  auto index = Upload(*device, *commands, words);
  const auto count = static_cast<uint32_t>(data.size() / stride);
  const uint64_t bytes = uint64_t{16} * count;
  auto result = Require(
      device->CreateBuffer({.Bytes = bytes, .Use = BufferUse::Storage | BufferUse::CopySource}));
  auto readback = Require(device->CreateBuffer(
      {.Bytes = bytes, .Use = BufferUse::CopyDestination, .Placement = Memory::Readback}));
  const std::array bindings{Storage(0, 0, *queries), Storage(0, 1, *index), Storage(1, 0, *result)};
  constexpr uint32_t kThreads = 64;
  Require(commands->Dispatch(*pipeline, bindings, {.X = (count + kThreads - 1) / kThreads}));
  Require(commands->Copy(*result, *readback, {.Bytes = bytes}));
  const auto token = Require(device->Submit(std::move(commands)));
  CHECK(Require(device->Wait(token, std::chrono::seconds(10))) == CompletionStatus::Ready,
        "bounded precision control completes");
  const auto mapped = Require(device->MapRead(*readback, {.Bytes = bytes}));
  std::ofstream output(argv[4], std::ios::binary);
  output.write(reinterpret_cast<const char *>(mapped.data()),
               static_cast<std::streamsize>(mapped.size()));
  CHECK(output.good(), "actual query answers retained");
  Require(device->Unmap(*readback));
  std::println("device={} queries={}", device->Limits().Name, count);
  return Report();
}
