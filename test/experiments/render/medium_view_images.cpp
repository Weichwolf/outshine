#include "MediumViewProbe.h"
#include <cstddef>
#include <cstdint>
#include "NativeGpuCheck.h"
#include "Check.h"
#include <vector>
#include <memory>
#include <algorithm>
#include <chrono>
#include <cstring>
#include <span>
#include <utility>

namespace outshine::Test::MediumViewProbe {
using namespace outshine::Test::Gpu;

void Finish(Device &device, std::unique_ptr<Commands> commands) {
  const auto completion = Require(device.Submit(std::move(commands)));
  CHECK(Require(device.Wait(completion, std::chrono::seconds(30))) == CompletionStatus::Ready,
        "native medium view operation completes within the offline test bound");
}

void UploadImage(Device &device,
                 Commands &commands,
                 Texture &texture,
                 std::span<const std::byte> bytes,
                 uint32_t rowBytes) {
  auto upload = Require(device.CreateBuffer(
      {.Bytes = bytes.size_bytes(), .Use = BufferUse::CopySource, .Placement = Memory::Upload}));
  const auto mapped = Require(device.MapWrite(*upload, {.Bytes = bytes.size_bytes()}));
  std::ranges::copy(bytes, mapped.begin());
  Require(device.Unmap(*upload));
  Require(commands.Copy(
      *upload,
      texture,
      {.RowBytes = rowBytes,
       .Region = {.Width = texture.Description().Width, .Height = texture.Description().Height}}));
}

std::vector<Pixel> ReadImage(Device &device, Buffer &readback) {
  const auto bytes = Require(device.MapRead(readback, {.Bytes = uint64_t{kRowBytes} * kHeight}));
  std::vector<Pixel> result(size_t{kWidth} * kHeight);
  for (uint32_t y = 0; y < kHeight; ++y) {
    std::memcpy(result.data() + size_t{y} * kWidth,
                bytes.data() + size_t{y} * kRowBytes,
                size_t{kWidth} * sizeof(Pixel));
  }
  Require(device.Unmap(readback));
  return result;
}
}
