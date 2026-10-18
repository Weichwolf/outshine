#ifndef OUTSHINE_TEST_RENDER_HEIGHTPAGEREADBACK_H
#define OUTSHINE_TEST_RENDER_HEIGHTPAGEREADBACK_H

#include "NativeGpuCheck.h"
#include "NativeOffline.h"
#include "NativeUploadFaultDevice.h"
#include <chrono>
#include <cstddef>
#include <cstring>
#include <span>
#include <vector>

namespace outshine::Test::Gpu {
inline std::vector<float> ReadHeightPage(UploadFailure &failure,
                                         Render::Native::Texture &atlas,
                                         Render::Native::TextureRegion region) {
  using namespace Render::Native;
  CHECK(failure.Observed != nullptr, "the real native upload device is observed");
  if (failure.Observed == nullptr) { Failed(); }
  auto &device = *failure.Observed;
  const uint64_t bytes = uint64_t{region.Width} * region.Height * sizeof(float);
  auto readback = Require(device.CreateBuffer(
      {.Bytes = bytes, .Use = BufferUse::CopyDestination, .Placement = Memory::Readback}));
  auto commands = Require(RetryOffline([&] { return device.Begin(); }, std::chrono::seconds(5)));
  Require(commands->Copy(
      atlas, *readback, {.RowBytes = uint64_t{region.Width} * sizeof(float), .Region = region}));
  const auto submitted = Require(device.Submit(std::move(commands)));
  CHECK(Require(device.Wait(submitted, std::chrono::seconds(5))) == CompletionStatus::Ready,
        "the GPU finishes the actual atlas readback");
  const auto mapped = Require(device.MapRead(*readback, {.Bytes = bytes}));
  std::vector<float> values(static_cast<size_t>(bytes) / sizeof(float));
  std::memcpy(values.data(), mapped.data(), static_cast<size_t>(bytes));
  Require(device.Unmap(*readback));
  return values;
}
}
#endif
