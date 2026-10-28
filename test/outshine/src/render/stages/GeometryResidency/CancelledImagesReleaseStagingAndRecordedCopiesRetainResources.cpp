#include "Check.h"
#include "GeometryResidency.h"
#include "test/harness/render/NativeGpuCheck.h"
#include <algorithm>
#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <span>
#include <utility>
#include <vector>

int main() {
  using namespace outshine::Render;
  using namespace outshine::Render::Native;
  using namespace outshine::Test;
  using namespace outshine::Test::Gpu;
  auto device = Require(CreateDevice({.Validation = true}));
  GeometryResidency residency;
  Require(residency.StandsOn(device.get()));
  constexpr auto deferred = GeometryResidency::ImageUploadMode::Deferred;
  constexpr auto linear = GeometryResidency::Transfer::Linear;
  constexpr size_t bytesPerImage = 20;
  const std::array<uint8_t, 16> pixels{
      64, 128, 192, 255, 64, 128, 192, 255, 64, 128, 192, 255, 64, 128, 192, 255};
  const MaterialTexture texture{.Rgba = pixels.data(), .Width = 2, .Height = 2, .Uv = {}};
  std::vector<GeometryResidency::BoundImage> images;
  images.reserve(6);
  for (size_t at = 0; at < 6; ++at) {
    images.push_back(Require(residency.Upload(texture, linear, TexelKind::Value, deferred)));
  }
  CHECK(residency.Allocations().TransferBytes == 6 * bytesPerImage,
        "all six base-plus-mip uploads are owned before submission");
  for (const size_t which : {size_t{2}, size_t{5}}) {
    auto retired = std::move(images[which].Image);
    const std::weak_ptr<Texture> weak = retired;
    residency.DiscardUnusedImage(retired);
    retired.reset();
    CHECK(weak.expired(), "an interior cancelled image loses both its transfer and texture owner");
  }
  CHECK(residency.Allocations().TransferBytes == 4 * bytesPerImage,
        "swap removal repairs the moved pending-image index");
  const auto sibling = images[0].Image;
  residency.DiscardUnusedImage(sibling);
  CHECK(residency.Allocations().TransferBytes == 4 * bytesPerImage,
        "another live binding protects its unsubmitted image");
  images[3].Image.reset();
  residency.DiscardUnreferencedImages();
  CHECK(residency.Allocations().TransferBytes == 3 * bytesPerImage,
        "failed-binding cleanup removes only orphaned transfers");
  Require(residency.SubmitPendingUploads());
  CHECK(residency.Allocations().TransferBytes == 0 &&
            residency.TakeUploadBytes() == 3 * bytesPerImage,
        "only the surviving images are uploaded");
  constexpr uint64_t rowBytes = 256;
  auto readback = Require(device->CreateBuffer(
      {.Bytes = rowBytes * 2, .Use = BufferUse::CopyDestination, .Placement = Memory::Readback}));
  auto commands = Require(device->Begin());
  Require(commands->Copy(
      *images[4].Image, *readback, {.RowBytes = rowBytes, .Region = {.Width = 2, .Height = 2}}));
  const std::weak_ptr<Texture> recordedImage = images[4].Image;
  images[4].Image.reset();
  CHECK(recordedImage.expired(),
        "the final public image wrapper is released before recorded GPU work is submitted");
  const auto submitted = Require(device->Submit(std::move(commands)));
  CHECK(Require(device->Wait(submitted, std::chrono::seconds(5))) == CompletionStatus::Ready,
        "recorded copies retain the native resource through successful completion");
  const auto mapped = Require(device->MapRead(*readback, {.Bytes = rowBytes * 2}));
  const auto expected = std::as_bytes(std::span(pixels));
  CHECK(mapped.size() >= rowBytes + 8 && std::ranges::equal(mapped.first(8), expected.first(8)) &&
            std::ranges::equal(mapped.subspan(rowBytes, 8), expected.subspan(8, 8)),
        "both actual GPU rows preserve their source pixels after wrapper retirement");
  Require(device->Unmap(*readback));
  return Report();
}
