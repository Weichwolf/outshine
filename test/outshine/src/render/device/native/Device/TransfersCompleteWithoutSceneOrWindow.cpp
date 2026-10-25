#include "Device.h"
#include "Check.h"
#include <algorithm>
#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <span>
#include <utility>

namespace {
using namespace outshine::Render::Native;
using namespace outshine::Test;
constexpr size_t kBytes = 64;

void Transfer() {
  const auto device = CreateDevice();
  CHECK(device, "native GPU creation needs no scene, content or window");
  if (!device) { return; }
  auto &gpu = **device;
  CHECK(!gpu.CreateBuffer({.Bytes = 0, .Use = BufferUse::CopySource}),
        "invalid allocation is rejected at the native boundary");
  const auto source = gpu.CreateBuffer(
      {.Bytes = kBytes, .Use = BufferUse::CopySource, .Placement = Memory::Upload});
  const auto resident = gpu.CreateBuffer(
      {.Bytes = kBytes, .Use = BufferUse::CopyDestination | BufferUse::CopySource});
  const auto result = gpu.CreateBuffer(
      {.Bytes = kBytes, .Use = BufferUse::CopyDestination, .Placement = Memory::Readback});
  CHECK(source && resident && result, "three native memory roles allocate independently");
  if (!source || !resident || !result) { return; }
  constexpr ByteRange range{.Offset = 0, .Bytes = kBytes};
  std::array<std::byte, kBytes> expected{};
  for (size_t at = 0; at < expected.size(); ++at) { expected[at] = static_cast<std::byte>(at); }
  const auto mapped = gpu.MapWrite(**source, range);
  CHECK(mapped && mapped->size() == expected.size(), "upload mapping exposes the bounded range");
  if (!mapped || mapped->size() != expected.size()) { return; }
  std::ranges::copy(expected, mapped->begin());
  CHECK(gpu.Unmap(**source), "upload mapping is released before recording");
  auto commands = gpu.Begin();
  CHECK(commands, "a standalone native command buffer records transfers");
  if (!commands) { return; }
  CHECK((*commands)->Copy(**source, **resident, {.Bytes = kBytes}) &&
            (*commands)->Copy(**resident, **result, {.Bytes = kBytes}),
        "upload and readback traverse actual device storage");
  const auto completion = gpu.Submit(std::move(*commands));
  CHECK(completion, "the native transfer has a completion fence");
  if (!completion) { return; }
  const auto ready = gpu.Wait(*completion, std::chrono::seconds(5));
  CHECK(ready && *ready == CompletionStatus::Ready, "transfer completes within the bounded wait");
  if (!ready || *ready != CompletionStatus::Ready) { return; }
  const auto read = gpu.MapRead(**result, range);
  CHECK(read && std::ranges::equal(*read, expected), "completed bytes match the original input");
  if (read) { CHECK(gpu.Unmap(**result), "readback mapping releases its ownership"); }
}
}

int main() {
  Transfer();
  return outshine::Test::Report();
}
