#include "Check.h"
#include "GeometryResidency.h"
#include "ReadbackTransfer.h"
#include "test/harness/render/NativeDeviceProxy.h"
#include "test/harness/render/NativeGpuCheck.h"
#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <expected>
#include <memory>
#include <span>
#include <string_view>

namespace {
using namespace outshine::Render;
using namespace outshine::Render::Native;
using namespace outshine::Test;
using namespace outshine::Test::Gpu;

struct Limited final : NativeDeviceProxy {
  explicit Limited(Device &device) : NativeDeviceProxy(device) {}

  uint64_t VertexBudget = 192;
  unsigned Refused = 0;

  Result<std::unique_ptr<Buffer>> CreateBuffer(BufferDescription description,
                                               std::string_view label = {}) override {
    if ((static_cast<uint32_t>(description.Use) & static_cast<uint32_t>(BufferUse::Vertex)) != 0 &&
        description.Bytes > VertexBudget) {
      ++Refused;
      return std::unexpected(
          Error{.Kind = ErrorKind::OutOfMemory, .Message = "fixture vertex allocation budget"});
    }
    return Backing().CreateBuffer(description, label);
  }
};

void Append(GeometryResidency &resident, std::span<const uint32_t> data, uint32_t offset) {
  GeometryResidency::Crossing crossing{.Which = GeometryResidency::Stream::Vertex,
                                       .Usage = BufferUse::Vertex,
                                       .From = data.data(),
                                       .Bytes = static_cast<uint32_t>(data.size_bytes()),
                                       .Offset = offset};
  Require(resident.Cross(std::span(&crossing, 1), true));
}
}

int main() {
  auto native = Require(CreateDevice({.Validation = true}));
  Limited device(*native);
  GeometryResidency resident;
  Require(resident.StandsOn(&device));
  constexpr auto stream = GeometryResidency::Stream::Vertex;
  std::array<uint32_t, 49> words{};
  for (size_t i = 0; i < words.size(); ++i) { words[i] = static_cast<uint32_t>(i * 17 + 3); }
  Append(resident, std::span(words).first<16>(), 0);
  const auto large =
      resident.Grow(stream, {.Usage = BufferUse::Vertex, .Bytes = 192, .Deferred = true});
  CHECK(large.has_value(),
        "a reservation larger than twice the old range fits its exact allocation budget");
  if (!large) { return Report(); }
  CHECK(device.Refused == 0 && resident.Buffer(stream)->Description().Bytes == 192,
        "the explicit large reservation requests no unused intermediate power of two");
  Append(resident, std::span(words).subspan<16, 32>(), 64);
  const auto *accepted = resident.Buffer(stream).get();
  const auto before = resident.Allocations();
  const auto denied =
      resident.Grow(stream, {.Usage = BufferUse::Vertex, .Bytes = 196, .Deferred = true});
  CHECK(!denied && denied.error().Kind == ErrorKind::OutOfMemory && device.Refused == 1,
        "ordinary geometric headroom still respects actual device admission failure");
  CHECK(resident.Buffer(stream).get() == accepted &&
            resident.Allocations().DeviceCopyBytes == before.DeviceCopyBytes &&
            resident.Allocations().RetiredDeviceBytes == before.RetiredDeviceBytes,
        "refused growth preserves the accepted target and all staged copy ownership");
  device.VertexBudget = 384;
  Require(resident.Grow(stream, {.Usage = BufferUse::Vertex, .Bytes = 196, .Deferred = true}));
  Append(resident, std::span(words).last<1>(), 192);
  Require(resident.SubmitPendingUploads());
  resident.ReleaseUploadScratch();
  auto readback =
      Require(ReadbackTransfer::Start(device, *resident.Buffer(stream), {.Bytes = sizeof(words)}));
  const auto read = Require(readback.Wait(device, std::chrono::seconds(5)));
  CHECK(read.has_value(), "the accepted retry finishes on the native device");
  if (read) {
    std::array<uint32_t, 49> copied{};
    std::memcpy(copied.data(), read->data(), sizeof(copied));
    CHECK(copied == words, "every accepted byte survives large growth, refusal and retry");
  }
  return Report();
}
