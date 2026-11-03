#include "Check.h"
#include "Device.h"
#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <utility>

int main() {
  using namespace outshine::Render::Native;
  using namespace outshine::Test;
  const auto device = CreateDevice({.Validation = true});
  CHECK(device, "pitched copies use a native device without renderer dependencies");
  if (!device) { return Report(); }
  auto &gpu = **device;
  constexpr std::array<uint32_t, 8> values{
      0, 0x3f800000, 0x40000000, 0x40400000, 0, 0x40800000, 0x40a00000, 0x40c00000};
  constexpr uint32_t canary = 0xcccccccc;
  std::array<uint32_t, 8> initial;
  initial.fill(canary);
  auto source = gpu.CreateBuffer(
      {.Bytes = sizeof(values), .Use = BufferUse::CopySource, .Placement = Memory::Upload});
  auto clear = gpu.CreateBuffer(
      {.Bytes = sizeof(initial), .Use = BufferUse::CopySource, .Placement = Memory::Upload});
  auto result = gpu.CreateBuffer(
      {.Bytes = sizeof(values), .Use = BufferUse::CopyDestination, .Placement = Memory::Readback});
  auto tooShort = gpu.CreateBuffer(
      {.Bytes = sizeof(values) - 1, .Use = BufferUse::CopySource | BufferUse::CopyDestination});
  auto texture = gpu.CreateTexture({.Width = 3,
                                    .Height = 2,
                                    .Texels = Format::R32Float,
                                    .Use = TextureUse::CopySource | TextureUse::CopyDestination});
  CHECK(source && clear && result && tooShort && texture, "exact and truncated transfers allocate");
  if (!source || !clear || !result || !tooShort || !texture) { return Report(); }
  auto sourceBytes = gpu.MapWrite(**source, {.Bytes = sizeof(values)});
  auto clearBytes = gpu.MapWrite(**clear, {.Bytes = sizeof(initial)});
  CHECK(sourceBytes && clearBytes, "bounded source buffers map");
  if (!sourceBytes || !clearBytes) { return Report(); }
  std::memcpy(sourceBytes->data(), values.data(), sizeof(values));
  std::memcpy(clearBytes->data(), initial.data(), sizeof(initial));
  CHECK(gpu.Unmap(**source) && gpu.Unmap(**clear), "upload mappings release before recording");
  auto commands = gpu.Begin();
  CHECK(commands, "native transfer commands begin");
  if (!commands) { return Report(); }
  constexpr TextureTransfer pitched{.BufferOffset = sizeof(uint32_t),
                                    .RowBytes = 4 * sizeof(uint32_t),
                                    .Region = {.Width = 3, .Height = 2}};
  CHECK(!(*commands)->Copy(**tooShort, **texture, pitched),
        "one missing byte refuses upload before recording");
  CHECK(!(*commands)->Copy(**texture, **tooShort, pitched),
        "one missing byte refuses readback before recording");
  CHECK((*commands)->Copy(**clear, **result, {.Bytes = sizeof(values)}),
        "readback padding initializes");
  CHECK((*commands)->Copy(**source, **texture, pitched),
        "upload needs no padding after the last texel");
  CHECK((*commands)->Copy(**texture, **result, pitched),
        "readback needs no padding after the last texel");
  const auto completion = gpu.Submit(std::move(*commands));
  CHECK(completion, "pitched transfer submits");
  if (!completion) { return Report(); }
  const auto ready = gpu.Wait(*completion, std::chrono::seconds(5));
  CHECK(ready && *ready == CompletionStatus::Ready,
        "pitched copies complete on the native backend");
  if (!ready || *ready != CompletionStatus::Ready) { return Report(); }
  const auto bytes = gpu.MapRead(**result, {.Bytes = sizeof(values)});
  CHECK(bytes && bytes->size() == sizeof(values), "complete native readback maps");
  if (!bytes || bytes->size() != sizeof(values)) { return Report(); }
  std::array<uint32_t, 8> actual{};
  std::memcpy(actual.data(), bytes->data(), sizeof(actual));
  auto expected = values;
  expected[0] = canary;
  expected[4] = canary;
  CHECK(actual == expected, "six texels survive; row padding and leading bytes stay untouched");
  CHECK(gpu.Unmap(**result), "readback mapping releases");
  return Report();
}
