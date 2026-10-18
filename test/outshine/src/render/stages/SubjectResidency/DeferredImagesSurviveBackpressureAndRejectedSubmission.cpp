#include "Check.h"
#include "SubjectResidency.h"
#include "ReadbackTransfer.h"
#include "test/harness/render/NativeGpuCheck.h"
#include "test/harness/render/NativeDeviceProxy.h"
#include <algorithm>
#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
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

struct Faults final : NativeDeviceProxy {
  explicit Faults(Device &device) : NativeDeviceProxy(device) {}

  bool Busy = false, Reject = false;
  unsigned Begins = 0, Submits = 0;

  Result<std::unique_ptr<Commands>> Begin() override {
    ++Begins;
    if (Busy) {
      return std::unexpected(
          Error{.Kind = ErrorKind::Busy, .Message = "injected image backpressure"});
    }
    return Backing().Begin();
  }

  Result<Completion> Submit(std::unique_ptr<Commands> commands) override {
    ++Submits;
    if (Reject) {
      return std::unexpected(Error{.Kind = ErrorKind::Driver, .Message = "rejected image batch"});
    }
    return Backing().Submit(std::move(commands));
  }
};
}

int main() {
  auto actual = Require(CreateDevice({.Validation = true}));
  Faults device(*actual);
  SubjectResidency residency;
  Require(residency.StandsOn(&device));
  constexpr auto deferred = SubjectResidency::ImageUploadMode::Deferred;
  constexpr auto linear = SubjectResidency::Transfer::Linear;
  constexpr size_t count = 12;
  constexpr size_t bytesPerImage = 16 + 4;
  const std::array<uint8_t, 16> pixels{
      64, 128, 192, 255, 64, 128, 192, 255, 64, 128, 192, 255, 64, 128, 192, 255};
  std::vector<SubjectResidency::BoundImage> images;
  {
    auto source = pixels;
    const SubjectTexture texture{.Rgba = source.data(), .Width = 2, .Height = 2, .Uv = {}};
    device.Busy = true;
    for (size_t at = 0; at < count; ++at) {
      images.push_back(Require(residency.Upload(texture, linear, TexelKind::Value, deferred)));
    }
    source.fill(0);
  }
  CHECK(device.Begins == 0 && device.Submits == 0,
        "image preparation performs no GPU command acquisition or waiting");
  CHECK(residency.Allocations().TransferBytes == count * bytesPerImage,
        "owned staging accounts for every base and mip after source release");
  const auto busy = residency.SubmitPendingUploads();
  CHECK(!busy && busy.error().Kind == ErrorKind::Busy && device.Submits == 0,
        "busy acquisition leaves the complete batch pending without submitting");
  residency.ReleaseUploadScratch();
  CHECK(residency.Allocations().TransferBytes == count * bytesPerImage,
        "scratch release cannot discard an unsubmitted image");
  device.Busy = false;
  device.Reject = true;
  const auto rejected = residency.SubmitPendingUploads();
  CHECK(!rejected && rejected.error().Kind == ErrorKind::Driver && device.Submits == 1 &&
            residency.Allocations().TransferBytes == count * bytesPerImage,
        "rejected submission preserves all images and their original staging for retry");
  device.Reject = false;
  Require(residency.SubmitPendingUploads());
  CHECK(device.Begins == 3 && device.Submits == 2 && residency.Allocations().TransferBytes == 0 &&
            residency.TakeUploadBytes() == count * bytesPerImage,
        "one accepted submission owns all image transfers and releases CPU staging once");
  for (const auto &image : images) {
    for (uint32_t mip = 0; mip < 2; ++mip) {
      auto transfer = Require(ReadbackTransfer::Start(
          device, *image.Image, {.Mip = mip, .Width = 2u >> mip, .Height = 2u >> mip}));
      const auto read = Require(transfer.Wait(device, std::chrono::seconds(5)));
      CHECK(read && std::ranges::equal(*read,
                                       std::as_bytes(std::span(pixels)).first(mip == 0 ? 16 : 4)),
            "every base and mip retains the original texels across retry and source release");
    }
  }
  const auto shared = Require(residency.Upload({}, linear, TexelKind::Value, deferred));
  const auto ready = Require(residency.Upload({}, linear, TexelKind::Value));
  CHECK(shared.Image == ready.Image && residency.Allocations().TransferBytes == 0,
        "an immediate lookup flushes its pending default and shares the same image");
  return Report();
}
