#include "Check.h"
#include "GeometryResidency.h"
#include "ReadbackTransfer.h"
#include "test/harness/render/NativeDeviceProxy.h"
#include "test/harness/render/NativeGpuCheck.h"
#include <algorithm>
#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <expected>
#include <limits>
#include <memory>
#include <span>
#include <string_view>
#include <utility>

namespace {
using namespace outshine::Render;
using namespace outshine::Render::Native;
using namespace outshine::Test::Gpu;
constexpr auto kStream = GeometryResidency::Stream::Vertex;

struct Refusing final : NativeDeviceProxy {
  using NativeDeviceProxy::NativeDeviceProxy;
  bool RefuseBuffer = false, RefuseSubmit = false;

  Result<std::unique_ptr<Buffer>> CreateBuffer(BufferDescription description,
                                               std::string_view label = {}) override {
    if (RefuseBuffer) {
      return std::unexpected(Error{.Kind = ErrorKind::Busy, .Message = "buffer pressure"});
    }
    return Backing().CreateBuffer(description, label);
  }

  Result<Completion> Submit(std::unique_ptr<Commands> commands) override {
    if (RefuseSubmit) {
      return std::unexpected(Error{.Kind = ErrorKind::Busy, .Message = "submission pressure"});
    }
    return Backing().Submit(std::move(commands));
  }
};

void Append(GeometryResidency &resident, std::span<const uint32_t> words, uint32_t offset) {
  GeometryResidency::Crossing crossing{.Which = kStream,
                                       .Usage = BufferUse::Vertex,
                                       .From = words.data(),
                                       .Bytes = static_cast<uint32_t>(words.size_bytes()),
                                       .Offset = offset};
  Require(resident.Cross(std::span(&crossing, 1), true));
}

void Exercise(Refusing &device) {
  GeometryResidency resident;
  Require(resident.StandsOn(&device));
  constexpr std::array<uint32_t, 12> words{1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12};
  Append(resident, std::span(words).first<4>(), 0);
  Require(resident.Grow(kStream,
                        {.Usage = BufferUse::Vertex,
                         .Bytes = 64,
                         .Deferred = true,
                         .PreserveBytes = std::numeric_limits<uint64_t>::max()}));
  Append(resident, std::span(words).subspan<4, 4>(), 32);
  Require(resident.Grow(
      kStream, {.Usage = BufferUse::Vertex, .Bytes = 68, .Deferred = true, .PreserveBytes = 48}));
  CHECK(resident.HeldOf(kStream) == 128 && resident.Allocations().DeviceCopyBytes == 16 + 48,
        "known sparse prefix avoids reserve bytes without changing geometric capacity growth");
  Append(resident, std::span(words).last<4>(), 64);
  const auto *accepted = resident.Buffer(kStream).get();
  device.RefuseBuffer = true;
  const auto refused = resident.Grow(
      kStream, {.Usage = BufferUse::Vertex, .Bytes = 260, .Deferred = true, .PreserveBytes = 80});
  CHECK(!refused && refused.error().Kind == ErrorKind::Busy &&
            resident.Buffer(kStream).get() == accepted &&
            resident.Allocations().DeviceCopyBytes == 64,
        "allocation pressure preserves the original buffer and does not schedule a copy");
  device.RefuseBuffer = false;
  Require(resident.Grow(
      kStream, {.Usage = BufferUse::Vertex, .Bytes = 260, .Deferred = true, .PreserveBytes = 80}));
  CHECK(resident.Allocations().DeviceCopyBytes == 144 &&
            resident.Allocations().RetiredDeviceBytes == 16 + 64 + 128,
        "both known prefixes and every source backing remain owned until submission");
  device.RefuseSubmit = true;
  CHECK(!resident.SubmitPendingUploads(), "rejected submission retains queued prefix copies");
  resident.ReleaseUploadScratch();
  CHECK(resident.Allocations().RetiredDeviceBytes == 208,
        "rejected submission cannot release buffers referenced by pending copies");
  device.RefuseSubmit = false;
  Require(resident.SubmitPendingUploads());
  CHECK(resident.Allocations().RetiredDeviceBytes == 0,
        "accepted submission transfers old buffer ownership to native completion tracking");
  auto transfer =
      Require(ReadbackTransfer::Start(device, *resident.Buffer(kStream), {.Bytes = 80}));
  const auto read = Require(transfer.Wait(device, std::chrono::seconds(5)));
  CHECK(read, "final native prefix is readable after retry");
  if (read) {
    std::array<uint32_t, 20> copied;
    std::memcpy(copied.data(), read->data(), sizeof(copied));
    for (size_t part = 0; part < 3; ++part) {
      CHECK(std::ranges::equal(std::span(copied).subspan(part * 8, 4),
                               std::span(words).subspan(part * 4, 4)),
            "every populated sparse range survives prefix growth and submission retry");
    }
  }
  const auto before = resident.Allocations().DeviceCopyBytes;
  Require(resident.Grow(
      kStream, {.Usage = BufferUse::Vertex, .Bytes = 1024, .Deferred = true, .PreserveBytes = 0}));
  CHECK(resident.Allocations().DeviceCopyBytes == before,
        "an explicitly empty live prefix does not schedule a preservation copy");
}
}

int main() {
  auto actual = Require(CreateDevice({.Validation = true}));
  Refusing device(*actual);
  Exercise(device);
  return outshine::Test::Report();
}
