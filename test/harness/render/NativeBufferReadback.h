#ifndef OUTSHINE_TEST_RENDER_NATIVEBUFFERREADBACK_H
#define OUTSHINE_TEST_RENDER_NATIVEBUFFERREADBACK_H

#include "NativeGpuCheck.h"
#include "NativeOffline.h"
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <utility>
#include <vector>

namespace outshine::Test::Gpu {
inline std::vector<uint32_t>
ReadBufferWords(Device &device, const Buffer &buffer, uint64_t offset, uint64_t bytes) {
  auto commands = Require(RetryOffline([&] { return device.Begin(); }, std::chrono::seconds(5)));
  auto read = Require(device.CreateBuffer(
      {.Bytes = bytes, .Use = BufferUse::CopyDestination, .Placement = Memory::Readback}));
  Require(commands->Copy(buffer, *read, {.SourceOffset = offset, .Bytes = bytes}));
  const auto done = Require(device.Submit(std::move(commands)));
  CHECK(Require(device.Wait(done, std::chrono::seconds(5))) == CompletionStatus::Ready,
        "native patch readback completes");
  const auto mapped = Require(device.MapRead(*read, {.Bytes = bytes}));
  std::vector<uint32_t> result(bytes / sizeof(uint32_t));
  std::memcpy(result.data(), mapped.data(), static_cast<size_t>(bytes));
  Require(device.Unmap(*read));
  return result;
}

}
#endif
