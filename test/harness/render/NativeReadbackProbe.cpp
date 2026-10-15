#include "src/render/device/ReadbackTransfer.h"
#include "NativeGpuCheck.h"
#include "Check.h"
#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>
#include <memory>
#include <optional>
#include <span>
#include <thread>
#include <utility>

namespace {
using namespace outshine::Render;
using namespace outshine::Render::Native;
using namespace outshine::Test;
using namespace outshine::Test::Gpu;
constexpr uint32_t kHeldValue = 12345;

void Complete(Device &device, Completion completion) {
  CHECK(Require(device.Wait(completion, std::chrono::seconds(5))) == CompletionStatus::Ready,
        "bounded test preparation completes");
}

std::unique_ptr<Buffer> BufferOf(Device &device, std::span<const uint32_t> values) {
  auto commands = Require(device.Begin());
  auto buffer = Upload(device, *commands, values);
  Complete(device, Require(device.Submit(std::move(commands))));
  return buffer;
}

void BufferRanges(Device &device) {
  const std::array<uint32_t, 5> values{11, 22, 33, 44, 55};
  auto buffer = BufferOf(device, values);
  auto transfer = Require(ReadbackTransfer::Start(
      device, *buffer, {.Offset = sizeof(uint32_t), .Bytes = 3 * sizeof(uint32_t)}));
  CHECK(transfer.RowBytes() == 3 * sizeof(uint32_t), "buffer range has an exact byte extent");
  buffer.reset();
  const auto mapped = Require(transfer.Wait(device, std::chrono::seconds(5)));
  CHECK(mapped && mapped->size() == 3 * sizeof(uint32_t),
        "the accepted readback retains its source after wrapper release");
  if (!mapped) { Failed(); }
  std::array<uint32_t, 3> actual{};
  std::memcpy(actual.data(), mapped->data(), sizeof(actual));
  CHECK((actual == std::array<uint32_t, 3>{22, 33, 44}),
        "only the requested source bytes reach the host");
  auto moved = std::move(transfer);
  auto again = Require(moved.Poll(device));
  CHECK(again && again->data() == mapped->data(),
        "moving a mapped transfer preserves the owned byte lifetime");
  auto foreign = Require(CreateDevice());
  const auto wrong = moved.Poll(*foreign);
  CHECK(!wrong && wrong.error().Kind == ErrorKind::Invalid,
        "cached mapped bytes cannot bypass device ownership");
  bool wrongThread = false;
  std::thread other([&] {
    auto result = moved.Poll(device);
    wrongThread = !result && result.error().Kind == ErrorKind::Invalid;
  });
  other.join();
  CHECK(wrongThread, "a cached readback preserves render owner access");
}

void ImageRegion(Device &device) {
  constexpr uint32_t width = 4;
  constexpr uint32_t height = 3;
  const std::array<uint32_t, size_t{width} * height> pixels{0xff000001u,
                                                            0xff000002u,
                                                            0xff000003u,
                                                            0xff000004u,
                                                            0xff000005u,
                                                            0xff000006u,
                                                            0xff000007u,
                                                            0xff000008u,
                                                            0xff000009u,
                                                            0xff00000au,
                                                            0xff00000bu,
                                                            0xff00000cu};
  auto texture =
      Require(device.CreateTexture({.Width = 8,
                                    .Height = 6,
                                    .Mips = 2,
                                    .Layers = 2,
                                    .Type = TextureType::Image2DArray,
                                    .Texels = Format::Rgba8Unorm,
                                    .Use = TextureUse::CopySource | TextureUse::CopyDestination}));
  auto upload = Require(device.CreateBuffer(
      {.Bytes = sizeof(pixels), .Use = BufferUse::CopySource, .Placement = Memory::Upload}));
  auto bytes = Require(device.MapWrite(*upload, {.Bytes = sizeof(pixels)}));
  std::memcpy(bytes.data(), pixels.data(), sizeof(pixels));
  Require(device.Unmap(*upload));
  auto commands = Require(device.Begin());
  Require(commands->Copy(*upload,
                         *texture,
                         {.RowBytes = width * sizeof(uint32_t),
                          .Region = {.Mip = 1, .Layer = 1, .Width = width, .Height = height}}));
  Complete(device, Require(device.Submit(std::move(commands))));
  auto transfer = Require(ReadbackTransfer::Start(
      device, *texture, {.Mip = 1, .Layer = 1, .X = 1, .Y = 1, .Width = 3, .Height = 2}));
  texture.reset();
  const auto result = Require(transfer.Wait(device, std::chrono::seconds(5)));
  CHECK(result && result->size() == 6 * sizeof(uint32_t) &&
            transfer.RowBytes() == 3 * sizeof(uint32_t),
        "mip, array layer and cropped rows have an exact packed output");
  if (!result) { Failed(); }
  std::array<uint32_t, 6> actual{};
  std::memcpy(actual.data(), result->data(), sizeof(actual));
  CHECK(
      (actual ==
       std::array<uint32_t, 6>{pixels[5], pixels[6], pixels[7], pixels[9], pixels[10], pixels[11]}),
      "readback preserves image component bytes and the declared source subregion");
}

class DeclaredTexture final : public Texture {
public:
  explicit DeclaredTexture(TextureDescription description) : Texture(description) {}
};

void InvalidRegions(Device &device) {
  constexpr uint32_t maximum = std::numeric_limits<uint32_t>::max();
  const DeclaredTexture oversized({.Width = maximum,
                                   .Height = maximum,
                                   .Texels = Format::Rgba32Float,
                                   .Use = TextureUse::CopySource});
  const auto overflow =
      ReadbackTransfer::Start(device, oversized, {.Width = maximum, .Height = maximum});
  CHECK(!overflow && overflow.error().Kind == ErrorKind::Invalid,
        "unrepresentable image byte products fail before allocation or commands");
  const DeclaredTexture image(
      {.Width = 8, .Height = 8, .Texels = Format::Rgba8Unorm, .Use = TextureUse::CopySource});
  const auto outside = ReadbackTransfer::Start(device, image, {.X = 7, .Width = 2, .Height = 1});
  CHECK(!outside && outside.error().Kind == ErrorKind::Invalid,
        "crop bounds are validated before native resource access");
  const DeclaredTexture badMip({.Width = 1,
                                .Height = 1,
                                .Mips = maximum,
                                .Texels = Format::R8Unorm,
                                .Use = TextureUse::CopySource});
  const auto shifted =
      ReadbackTransfer::Start(device, badMip, {.Mip = 32, .Width = 1, .Height = 1});
  CHECK(!shifted && shifted.error().Kind == ErrorKind::Invalid,
        "invalid mip metadata cannot cause an oversized shift");
  auto buffer = BufferOf(device, std::array<uint32_t, 1>{7});
  const auto range = ReadbackTransfer::Start(device, *buffer, {.Offset = 4, .Bytes = 1});
  CHECK(!range && range.error().Kind == ErrorKind::Invalid,
        "a buffer range past its end is rejected");
}

void PendingAndCancel(Device &device, const char *shader) {
  auto buffer = BufferOf(device, std::array<uint32_t, 1>{7});
  auto pipeline = Require(device.CreateComputePipeline(Spirv(shader)));
  auto commands = Require(device.Begin());
  const std::array bindings{Storage(0, 0, *buffer)};
  Require(commands->Dispatch(*pipeline, bindings, {.X = 1, .Y = 1, .Z = 1}));
  const auto slow = Require(device.Submit(std::move(commands)));
  auto transfer = Require(ReadbackTransfer::Start(device, *buffer, {.Bytes = sizeof(uint32_t)}));
  CHECK(!Require(transfer.Poll(device)), "pending GPU data is never exposed as a partial span");
  CHECK(!Require(transfer.Wait(device, std::chrono::nanoseconds(0))),
        "zero-budget waiting remains nonblocking");
  {
    auto abandoned = Require(ReadbackTransfer::Start(device, *buffer, {.Bytes = sizeof(uint32_t)}));
  }
  buffer.reset();
  Complete(device, slow);
  CHECK(Require(transfer.Wait(device, std::chrono::seconds(5))).has_value(),
        "abandoning another request preserves the accepted source and destination work");
}

void MappedOwnerLifetime() {
  std::optional<ReadbackTransfer> late;
  std::span<const std::byte> held;
  {
    auto device = Require(CreateDevice({.Validation = true}));
    auto buffer = BufferOf(*device, std::array<uint32_t, 1>{kHeldValue});
    late.emplace(Require(ReadbackTransfer::Start(*device, *buffer, {.Bytes = sizeof(uint32_t)})));
    auto mapped = Require(late->Wait(*device, std::chrono::seconds(5)));
    CHECK(mapped.has_value(), "completed readback owns its mapped allocation");
    if (!mapped) { Failed(); }
    held = *mapped;
  }
  uint32_t value = 0;
  std::memcpy(&value, held.data(), sizeof(value));
  CHECK(value == kHeldValue,
        "mapped data outlives the device facade through native resource ownership");
  late.reset();
}

}

int main(int argc, char **argv) {
  if (argc != 2) { return 2; }
  auto device = Require(CreateDevice({.Validation = true}));
  BufferRanges(*device);
  ImageRegion(*device);
  InvalidRegions(*device);
  PendingAndCancel(*device, argv[1]);
  MappedOwnerLifetime();
  return Report();
}
