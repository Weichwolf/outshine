#include "CloudMomentProbe.h"
#include "NativeGpuCheck.h"
#include "Check.h"

#include <bit>
#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <ios>
#include <memory>
#include <print>
#include <span>
#include <utility>
#include <vector>

namespace {
using namespace outshine::Test;
using namespace outshine::Test::Gpu;
using namespace outshine::Test::CloudMomentProbe;
static_assert(std::endian::native == std::endian::little);
constexpr size_t kMaximumQueries = 65535;

std::vector<uint32_t> Read(const char *path) {
  std::ifstream file(path, std::ios::binary | std::ios::ate);
  const auto bytes = file ? static_cast<std::streamoff>(file.tellg()) : -1;
  const bool complete = bytes > 0 && std::cmp_less_equal(bytes, kMaximumQueries * sizeof(Query)) &&
                        static_cast<size_t>(bytes) % sizeof(Query) == 0;
  CHECK(complete, "the private fixture contains a bounded complete number of query records");
  if (!complete) { Failed(); }
  std::vector<uint32_t> result(static_cast<size_t>(bytes) / sizeof(uint32_t));
  file.seekg(0);
  CHECK(file.read(reinterpret_cast<char *>(result.data()), bytes), "all query bytes are read");
  return result;
}

std::vector<uint32_t>
Run(Device &device, const char *shader, const std::vector<uint32_t> &queries) {
  const auto count = queries.size() * sizeof(uint32_t) / sizeof(Query);
  const auto bytes = count * sizeof(Answer);
  auto pipeline = Require(device.CreateComputePipeline(Spirv(shader)));
  auto commands = Require(device.Begin());
  auto input = Upload(device, *commands, queries);
  auto output = Require(
      device.CreateBuffer({.Bytes = bytes, .Use = BufferUse::Storage | BufferUse::CopySource}));
  auto readback = Require(device.CreateBuffer(
      {.Bytes = bytes, .Use = BufferUse::CopyDestination, .Placement = Memory::Readback}));
  const std::array bindings{Storage(0, 0, *input), Storage(1, 0, *output)};
  Require(commands->Dispatch(*pipeline, bindings, {.X = static_cast<uint32_t>(count)}));
  Require(commands->Copy(*output, *readback, {.Bytes = bytes}));
  input.reset();
  output.reset();
  pipeline.reset();
  const auto completion = Require(device.Submit(std::move(commands)));
  CHECK(Require(device.Wait(completion, std::chrono::seconds(30))) == CompletionStatus::Ready,
        "the private GPU probe completes within its explicit offline bound");
  const auto mapped = Require(device.MapRead(*readback, {.Bytes = bytes}));
  std::vector<uint32_t> result(bytes / sizeof(uint32_t));
  std::memcpy(result.data(), mapped.data(), bytes);
  Require(device.Unmap(*readback));
  return result;
}
}

int main(int argc, char **argv) {
  if (argc != 4) {
    std::println(stderr, "usage: cloud-moments-native shader.spv queries.bin answers.bin");
    return 2;
  }
  const auto input = Read(argv[2]);
  auto device = Require(CreateDevice({.Validation = true}));
  const auto actual = Run(*device, argv[1], input);
  const auto repeated = Run(*device, argv[1], input);
  CHECK(actual == repeated, "independent native recordings produce identical moments/cells");
  std::ofstream output(argv[3], std::ios::binary);
  CHECK(output.write(reinterpret_cast<const char *>(actual.data()),
                     static_cast<std::streamsize>(actual.size() * sizeof(uint32_t))),
        "the original GPU moment and cell bytes are preserved");
  std::println("device={} queries={} input_bytes={} output_bytes={}",
               device->Limits().Name,
               input.size() * sizeof(uint32_t) / sizeof(Query),
               input.size() * sizeof(uint32_t),
               actual.size() * sizeof(uint32_t));
  return Report();
}
