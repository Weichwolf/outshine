#include "GeometryResidency.h"
#include "ReadbackTransfer.h"
#include "test/harness/render/NativeDeviceProxy.h"
#include "test/harness/render/NativeGpuCheck.h"
#include "test/harness/render/NativeImageReadback.h"
#include "Check.h"
#include <algorithm>
#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <expected>
#include <memory>
#include <span>
#include <utility>
#include <vector>

namespace {
using namespace outshine::Render;
using namespace outshine::Render::Native;
using namespace outshine::Test;
using namespace outshine::Test::Gpu;
constexpr auto kStream = GeometryResidency::Stream::Vertex;
constexpr auto kLinear = GeometryResidency::Transfer::Linear;
constexpr uint32_t kBlockBytes = 1024u * 1024u;
constexpr uint32_t kBlocks = 17;
constexpr uint32_t kBytes = kBlocks * kBlockBytes;
constexpr std::array<uint8_t, 16> kPixels{
    64, 128, 192, 255, 64, 128, 192, 255, 64, 128, 192, 255, 64, 128, 192, 255};

struct Faults final : NativeDeviceProxy {
  explicit Faults(Device &device) : NativeDeviceProxy(device) {}

  bool Busy = false, Reject = false;
  unsigned Begins = 0, Submits = 0, Waits = 0;

  Result<std::unique_ptr<Commands>> Begin() override {
    ++Begins;
    if (Busy) {
      return std::unexpected(Error{.Kind = ErrorKind::Busy, .Message = "no upload acquisition"});
    }
    return Backing().Begin();
  }

  Result<Completion> Submit(std::unique_ptr<Commands> commands) override {
    ++Submits;
    if (Reject) {
      return std::unexpected(Error{.Kind = ErrorKind::Driver, .Message = "rejected source"});
    }
    return Backing().Submit(std::move(commands));
  }

  Result<CompletionStatus> Wait(Completion completion, std::chrono::nanoseconds budget) override {
    ++Waits;
    return Backing().Wait(completion, budget);
  }
};

void StreamAndImage(Faults &device) {
  GeometryResidency source;
  Require(source.StandsOn(&device));
  auto commands = Require(device.Begin());
  GeometryResidency::BoundImage image;
  {
    const auto recording = Require(source.RecordUploads(*commands));
    const auto nested = source.RecordUploads(*commands);
    CHECK(!nested && nested.error().Kind == ErrorKind::Invalid,
          "a second recording cannot overwrite the active command owner");
    CHECK(!source.StandsOn(nullptr), "an active recording cannot drop its producing device");
    Faults alias(static_cast<Device &>(device));
    CHECK(!source.StandsOn(&alias), "an empty active recording cannot change device identity");
    device.Busy = true;
    Require(source.Grow(kStream, {.Usage = BufferUse::Vertex, .Bytes = 8 * kBlockBytes}));
    std::vector<uint32_t> pixels(kBlockBytes / sizeof(uint32_t));
    for (uint32_t block = 0; block < kBlocks; ++block) {
      if (block == 8) {
        Require(source.Grow(kStream, {.Usage = BufferUse::Vertex, .Bytes = kBytes}));
      }
      std::ranges::fill(pixels, block + 1);
      GeometryResidency::Crossing crossing{.Which = kStream,
                                           .Usage = BufferUse::Vertex,
                                           .From = pixels.data(),
                                           .Bytes = kBlockBytes,
                                           .Offset = block * kBlockBytes};
      Require(source.Cross(std::span(&crossing, 1), false));
    }
    image = Require(source.Upload(
        {.Rgba = kPixels.data(), .Width = 2, .Height = 2, .Uv = {}}, kLinear, TexelKind::Value));
    Require(source.SubmitPendingUploads());
    CHECK(device.Begins == 1 && device.Submits == 0 && device.Waits == 0,
          "growth, immediate copies, images and the staging threshold only record commands");
    source.ReleaseUploadScratch();
  }
  device.Busy = false;
  const auto completion = Require(device.Submit(std::move(commands)));
  CHECK(Require(device.Wait(completion, std::chrono::seconds(5))) == CompletionStatus::Ready,
        "the caller owns the sole explicit submission and offline completion wait");
  auto read = Require(ReadbackTransfer::Start(device, *source.Buffer(kStream), {.Bytes = kBytes}));
  const auto bytes = Require(read.Wait(device, std::chrono::seconds(5)));
  CHECK(bytes && bytes->size() == kBytes, "the whole source stream reaches native readback");
  size_t errors = 0;
  if (bytes) {
    for (size_t at = 0; at < bytes->size() / sizeof(uint32_t); ++at) {
      uint32_t value = 0;
      std::memcpy(&value, bytes->data() + at * sizeof(value), sizeof(value));
      if (value != at * sizeof(value) / kBlockBytes + 1) { ++errors; }
    }
  }
  CHECK(errors == 0, "every uploaded word survives buffer growth and staging reuse");
  const auto rows = ReadImage(device, *image.Image, {.Width = 2, .Height = 2});
  CHECK(std::memcmp(rows.Rows.data(), kPixels.data(), 8) == 0 &&
            std::memcmp(rows.Rows.data() + rows.RowBytes, kPixels.data() + 8, 8) == 0,
        "the material image joins the same source submission without losing either row");
  const auto submits = device.Submits;
  const uint32_t value = 91;
  GeometryResidency::Crossing immediate{
      .Which = kStream, .Usage = BufferUse::Vertex, .From = &value, .Bytes = sizeof(value)};
  Require(source.Cross(std::span(&immediate, 1), false));
  CHECK(device.Submits == submits + 1, "leaving the scope restores ordinary upload submission");
}

void CancelAndReject(Faults &device) {
  for (const bool reject : {false, true}) {
    GeometryResidency source;
    Require(source.StandsOn(&device));
    auto commands = Require(device.Begin());
    const auto submits = device.Submits;
    std::weak_ptr<Texture> imageWrapper;
    {
      const auto recording = Require(source.RecordUploads(*commands));
      const auto image = Require(source.Upload(
          {.Rgba = kPixels.data(), .Width = 2, .Height = 2, .Uv = {}}, kLinear, TexelKind::Value));
      imageWrapper = image.Image;
      Require(source.SubmitPendingUploads());
    }
    CHECK(imageWrapper.expired(),
          "recorded work retains native resources after wrapper retirement");
    CHECK(device.Submits == submits, "abandonable source construction never submits implicitly");
    if (reject) {
      device.Reject = true;
      const auto refused = device.Submit(std::move(commands));
      CHECK(!refused && refused.error().Kind == ErrorKind::Driver,
            "submission rejection remains an explicit native failure at the caller boundary");
      device.Reject = false;
    }
  }
}
}

int main() {
  auto actual = Require(CreateDevice({.Validation = true}));
  Faults device(*actual);
  StreamAndImage(device);
  CancelAndReject(device);
  return Report();
}
