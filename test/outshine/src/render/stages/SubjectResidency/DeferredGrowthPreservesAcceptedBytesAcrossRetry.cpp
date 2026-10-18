#include "Check.h"
#include "ReadbackTransfer.h"
#include "SubjectResidency.h"
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
#include <utility>

namespace {
using namespace outshine::Render;
using namespace outshine::Render::Native;
using namespace outshine::Test::Gpu;

struct Observed final : NativeDeviceProxy {
  explicit Observed(Device &device) : NativeDeviceProxy(device) {}

  unsigned Begins = 0;
  unsigned Submits = 0;
  bool Reject = false;

  Result<std::unique_ptr<Commands>> Begin() override {
    ++Begins;
    return Backing().Begin();
  }

  Result<Completion> Submit(std::unique_ptr<Commands> commands) override {
    ++Submits;
    if (Reject) {
      return std::unexpected(Error{.Kind = ErrorKind::Driver, .Message = "rejected batch"});
    }
    return Backing().Submit(std::move(commands));
  }
};

void Append(SubjectResidency &resident, std::span<const uint32_t> words, uint32_t offset) {
  SubjectResidency::Crossing crossing{.Which = SubjectResidency::Stream::Vertex,
                                      .Usage = BufferUse::Vertex,
                                      .From = words.data(),
                                      .Bytes = static_cast<uint32_t>(words.size_bytes()),
                                      .Offset = offset};
  Require(resident.Cross(std::span(&crossing, 1), true));
}
}

int main() {
  using namespace outshine::Test;
  auto actual = Require(CreateDevice({.Validation = true}));
  Observed device(*actual);
  SubjectResidency resident;
  Require(resident.StandsOn(&device));
  constexpr auto stream = SubjectResidency::Stream::Vertex;
  constexpr std::array<uint32_t, 16> words{1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16};
  Append(resident, std::span(words).first<4>(), 0);
  Require(resident.Grow(stream, {.Usage = BufferUse::Vertex, .Bytes = 32, .Deferred = true}));
  Append(resident, std::span(words).subspan<4, 4>(), 16);
  Require(resident.Grow(stream, {.Usage = BufferUse::Vertex, .Bytes = 64, .Deferred = true}));
  Append(resident, std::span(words).last<8>(), 32);
  CHECK(device.Begins == 0 && device.Submits == 0,
        "mesh growth and writes share the future frame without acquiring or submitting commands");
  CHECK(resident.Allocations().RetiredDeviceBytes == 48 && resident.HeldBytes() == 112,
        "both old allocations stay alive while queued copies reference them");
  CHECK(resident.Allocations().DeviceCopyBytes == 48,
        "preserving two growing prefixes accounts for the exact GPU copy bytes");
  device.Reject = true;
  const auto failed = resident.SubmitPendingUploads();
  CHECK(!failed && failed.error().Kind == ErrorKind::Driver,
        "an unaccepted submission remains an explicit error");
  const auto scratch = resident.Allocations().TransferBytes;
  resident.ReleaseUploadScratch();
  CHECK(resident.Allocations().TransferBytes == scratch &&
            resident.Allocations().RetiredDeviceBytes == 48,
        "failed submission retains sources and old targets for retry");
  device.Reject = false;
  Require(resident.SubmitPendingUploads());
  CHECK(device.Submits == 2 && resident.Allocations().RetiredDeviceBytes == 0 &&
            resident.HeldBytes() == 64,
        "one accepted retry transfers ownership of old resources to native completion tracking");
  resident.ReleaseUploadScratch();
  auto transfer = Require(ReadbackTransfer::Start(device, *resident.Buffer(stream), {.Bytes = 64}));
  const auto read = Require(transfer.Wait(device, std::chrono::seconds(5)));
  CHECK(read, "the accepted final allocation is independently readable");
  if (read) {
    std::array<uint32_t, 16> copied{};
    std::memcpy(copied.data(), read->data(), sizeof(copied));
    CHECK(copied == words,
          "every appended word survives deferred growth, rejected submission and scratch release");
  }
  return Report();
}
