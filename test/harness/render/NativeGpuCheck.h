#ifndef OUTSHINE_TEST_RENDER_NATIVEGPUCHECK_H
#define OUTSHINE_TEST_RENDER_NATIVEGPUCHECK_H
#include "Device.h"
#include "Check.h"
#include <cstdlib>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <print>
#include <span>
#include <vector>

namespace outshine::Test::Gpu {
using namespace outshine::Render::Native;

[[noreturn]] inline void Failed() {
  const int status = Report();
  std::fflush(stdout);
  std::_Exit(status);
}

inline void Require(Render::Native::Result<void> result) {
  CHECK(result, "native GPU operation succeeds");
  if (!result) {
    std::println(stderr, "{}", result.error().Message);
    Failed();
  }
}

template <typename T> T Require(Render::Native::Result<T> result) {
  CHECK(result, "native GPU operation returns a value");
  if (!result) {
    std::println(stderr, "{}", result.error().Message);
    Failed();
  }
  return std::move(*result);
}

inline std::vector<uint32_t> Spirv(const char *path) {
  std::ifstream file(path, std::ios::binary | std::ios::ate);
  constexpr std::streamoff maximum = 16L * 1024 * 1024;
  const std::streamoff bytes = file ? file.tellg() : std::streampos(-1);
  CHECK(bytes > 0 && bytes <= maximum && bytes % sizeof(uint32_t) == 0,
        "the shader has a bounded word-aligned size");
  if (bytes <= 0 || bytes > maximum || bytes % sizeof(uint32_t) != 0) { Failed(); }
  std::vector<uint32_t> words(static_cast<size_t>(bytes) / sizeof(uint32_t));
  file.seekg(0);
  file.read(reinterpret_cast<char *>(words.data()), bytes);
  CHECK(file, "the complete shader is readable");
  return words;
}

inline std::unique_ptr<Render::Native::Buffer>
Upload(Device &device, Commands &commands, std::span<const uint32_t> words) {
  auto upload = Require(device.CreateBuffer(
      {.Bytes = words.size_bytes(), .Use = BufferUse::CopySource, .Placement = Memory::Upload}));
  const auto mapped = Require(device.MapWrite(*upload, {.Bytes = words.size_bytes()}));
  std::memcpy(mapped.data(), words.data(), words.size_bytes());
  Require(device.Unmap(*upload));
  auto result = Require(device.CreateBuffer(
      {.Bytes = words.size_bytes(),
       .Use = BufferUse::Storage | BufferUse::CopyDestination | BufferUse::CopySource}));
  Require(commands.Copy(*upload, *result, {.Bytes = words.size_bytes()}));
  auto busy = device.MapWrite(*upload, {.Bytes = words.size_bytes()});
  CHECK(!busy && busy.error().Kind == ErrorKind::Busy, "recorded uploads cannot be rewritten");
  return result;
}

inline Binding Storage(uint32_t set, uint32_t slot, const Render::Native::Buffer &buffer) {
  return {.Set = set,
          .Slot = slot,
          .Value = StorageBuffer{.Data = &buffer, .Range = {.Bytes = buffer.Description().Bytes}}};
}

}
#endif
