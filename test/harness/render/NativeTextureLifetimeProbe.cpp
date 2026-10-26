#include "NativeGpuCheck.h"
#include "Check.h"
#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>
#include <memory>
#include <span>
#include <utility>
#include <string_view>
#include <thread>
#include <vector>

namespace {
using namespace outshine::Render::Native;
using namespace outshine::Test;
using namespace outshine::Test::Gpu;

constexpr std::byte kPadding{0xcd};
constexpr uint32_t kWidth = 8, kHeight = 4, kPitch = 64, kOffset = 16, kBytes = 512;

TextureTransfer Transfer() {
  return {.BufferOffset = kOffset,
          .RowBytes = kPitch,
          .Region = {.Layer = 1, .Width = kWidth, .Height = kHeight}};
}

std::vector<std::byte> Input() {
  std::vector<std::byte> bytes(kBytes, kPadding);
  for (uint32_t y = 0; y < kHeight; ++y) {
    for (uint32_t x = 0; x < kWidth; ++x) {
      const auto value = static_cast<float>(y * kWidth + x + 1);
      std::memcpy(
          bytes.data() + kOffset + size_t{y} * kPitch + x * sizeof(float), &value, sizeof(value));
    }
  }
  return bytes;
}

std::unique_ptr<Buffer> UploadPixels(Device &device) {
  auto upload = Require(device.CreateBuffer(
      {.Bytes = kBytes, .Use = BufferUse::CopySource, .Placement = Memory::Upload}));
  const auto mapped = Require(device.MapWrite(*upload, {.Bytes = kBytes}));
  const auto bytes = Input();
  std::memcpy(mapped.data(), bytes.data(), bytes.size());
  Require(device.Unmap(*upload));
  return upload;
}

std::unique_ptr<Texture> Image(Device &device) {
  return Require(device.CreateTexture(
      {.Width = kWidth,
       .Height = kHeight,
       .Mips = 3,
       .Layers = 2,
       .Type = TextureType::Image2DArray,
       .Texels = Format::R32Float,
       .Use = TextureUse::CopySource | TextureUse::CopyDestination | TextureUse::Sampled}));
}

void InvalidTransfers(Device &device, const Buffer &upload, Texture &image) {
  auto commands = Require(device.Begin());
  auto invalid = Transfer();
  invalid.RowBytes = kWidth * sizeof(float) - 1;
  const auto pitch = commands->Copy(upload, image, invalid);
  CHECK(!pitch && pitch.error().Kind == ErrorKind::Invalid, "short row pitch is rejected");
  invalid = Transfer();
  invalid.BufferOffset = 2;
  const auto offset = commands->Copy(upload, image, invalid);
  CHECK(!offset && offset.error().Kind == ErrorKind::Invalid,
        "misaligned pixel offset is rejected");
  invalid = Transfer();
  invalid.RowBytes = std::numeric_limits<uint64_t>::max() - 3;
  const auto overflow = commands->Copy(upload, image, invalid);
  CHECK(!overflow && overflow.error().Kind == ErrorKind::Invalid, "overflowing pitch is rejected");
  invalid = Transfer();
  invalid.Region.Height = 1;
  invalid.RowBytes = uint64_t{std::numeric_limits<int32_t>::max()} + 1;
  const auto nativePitch = commands->Copy(upload, image, invalid);
  CHECK(!nativePitch && nativePitch.error().Kind == ErrorKind::Invalid,
        "a one-row pitch still obeys the Vulkan signed-31-bit row-byte limit");
  invalid = Transfer();
  invalid.Region.X = std::numeric_limits<uint32_t>::max();
  const auto outside = commands->Copy(upload, image, invalid);
  CHECK(!outside && outside.error().Kind == ErrorKind::Invalid,
        "overflowing pixel region is rejected");
  invalid = Transfer();
  invalid.Region.Mip = 3;
  const auto mip = commands->Copy(upload, image, invalid);
  CHECK(!mip && mip.error().Kind == ErrorKind::Invalid, "missing mip is rejected");
  std::thread foreign([&] {
    const auto result = commands->Copy(upload, image, Transfer());
    CHECK(!result && result.error().Kind == ErrorKind::Invalid,
          "foreign-thread image copy is rejected");
  });
  foreign.join();
}

void Reservations(Device &device, const Buffer &upload, Texture &image, Buffer &readback) {
  auto cancelled = Require(device.Begin());
  Require(cancelled->Copy(upload, image, Transfer()));
  cancelled.reset();
  auto first = Require(device.Begin());
  Require(first->Copy(upload, image, Transfer()));
  auto second = Require(device.Begin());
  const auto busy = second->Copy(image, readback, Transfer());
  CHECK(!busy && busy.error().Kind == ErrorKind::Busy, "an unsubmitted image use has one recorder");
  second.reset();
  const auto completion = Require(device.Submit(std::move(first)));
  second = Require(device.Begin());
  Require(second->Copy(image, readback, Transfer()));
  CHECK(Require(device.Wait(completion, std::chrono::seconds(5))) == CompletionStatus::Ready,
        "first completion can retire while a later recording reserves the same image");
  auto third = Require(device.Begin());
  const auto stillBusy = third->Copy(image, readback, Transfer());
  CHECK(!stillBusy && stillBusy.error().Kind == ErrorKind::Busy,
        "retiring an older recording does not release a later image reservation");
  third.reset();
  second.reset();
}

void Pixels(Device &device, const Buffer &upload, Texture &image, Buffer &readback) {
  auto commands = Require(device.Begin());
  Require(commands->Copy(upload, readback, {.Bytes = kBytes}));
  Require(commands->Copy(image,
                         {.Layer = 1, .Width = 4, .Height = kHeight},
                         image,
                         {.Layer = 1, .X = 4, .Width = 4, .Height = kHeight}));
  const auto overlap = commands->Copy(image,
                                      {.Layer = 1, .Width = 4, .Height = kHeight},
                                      image,
                                      {.Layer = 1, .X = 2, .Width = 4, .Height = kHeight});
  CHECK(!overlap && overlap.error().Kind == ErrorKind::Invalid,
        "overlapping same-subresource copy is rejected");
  Require(commands->Copy(image, readback, Transfer()));
  const auto completion = Require(device.Submit(std::move(commands)));
  CHECK(Require(device.Wait(completion, std::chrono::seconds(5))) == CompletionStatus::Ready,
        "cross-submission layouts and a disjoint self-copy complete");
  auto expected = Input();
  for (uint32_t y = 0; y < kHeight; ++y) {
    for (uint32_t x = 4; x < kWidth; ++x) {
      const auto value = static_cast<float>(y * kWidth + x - 4 + 1);
      std::memcpy(expected.data() + kOffset + size_t{y} * kPitch + x * sizeof(float),
                  &value,
                  sizeof(value));
    }
  }
  const auto actual = Require(device.MapRead(readback, {.Bytes = kBytes}));
  for (size_t at = 0; at < expected.size(); ++at) {
    CHECK(actual[at] == expected[at], "pixels and all untouched offset/row/tail padding match");
  }
  Require(device.Unmap(readback));
}

void QueuedTransfers(Device &device,
                     const Buffer &upload,
                     std::unique_ptr<Texture> &image,
                     Buffer &readback) {
  auto first = Require(device.Begin());
  Require(first->Copy(upload, *image, Transfer()));
  const auto before = Require(device.Submit(std::move(first)));
  auto second = Require(device.Begin());
  Require(second->Copy(*image, readback, Transfer()));
  image.reset();
  const auto after = Require(device.Submit(std::move(second)));
  CHECK(Require(device.Wait(after, std::chrono::seconds(5))) == CompletionStatus::Ready,
        "dependent texture submissions complete without waiting between their submissions");
  CHECK(Require(device.Poll(before)) == CompletionStatus::Ready,
        "the ordered queue retires the preceding image upload");
  const auto actual = Require(device.MapRead(readback, {.Bytes = kBytes}));
  const auto expected = Input();
  for (size_t at = 0; at < expected.size(); ++at) {
    CHECK(actual[at] == expected[at],
          "queue-ordered texture readback retains image and exact contents");
  }
  Require(device.Unmap(readback));
}

void Aliases(Device &device, const char *path) {
  const auto pipeline = Require(device.CreateComputePipeline(Spirv(path)));
  const auto image =
      Require(device.CreateTexture({.Width = 8,
                                    .Height = 4,
                                    .Type = TextureType::Image2DArray,
                                    .Texels = Format::Rgba32Float,
                                    .Use = TextureUse::Sampled | TextureUse::Storage}));
  const auto sampler = Require(device.CreateSampler({}));
  const std::array<float, 8> uniform{};
  const std::array bindings{
      Binding{.Set = 0,
              .Slot = 0,
              .Value = SampledTexture{.Image = {.Image = image.get()}, .Sampling = sampler.get()}},
      Binding{.Set = 1, .Slot = 0, .Value = StorageTexture{.Image = {.Image = image.get()}}},
      Binding{.Set = 2, .Slot = 0, .Value = Uniform{std::as_bytes(std::span(uniform))}}};
  auto commands = Require(device.Begin());
  const auto alias = commands->Dispatch(*pipeline, bindings, {});
  CHECK(!alias && alias.error().Kind == ErrorKind::Invalid,
        "overlapping sampled and writable storage views fail before a dispatch");
}

}

int main(int argc, char *const *argv) {
  if ((argc != 2 && argc != 3) || (argc == 3 && std::string_view(argv[2]) != "--validation")) {
    return 1;
  }
  auto device = Require(CreateDevice({.Validation = argc == 3}));
  auto upload = UploadPixels(*device);
  auto image = Image(*device);
  auto readback = Require(device->CreateBuffer(
      {.Bytes = kBytes, .Use = BufferUse::CopyDestination, .Placement = Memory::Readback}));
  InvalidTransfers(*device, *upload, *image);
  Reservations(*device, *upload, *image, *readback);
  Pixels(*device, *upload, *image, *readback);
  QueuedTransfers(*device, *upload, image, *readback);
  Aliases(*device, argv[1]);
  return Report();
}
