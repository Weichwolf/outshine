#include "src/render/stages/SubjectResidency.h"
#include "src/render/device/ReadbackTransfer.h"
#include "NativeDeviceProxy.h"
#include "NativeGpuCheck.h"
#include "NativeOffline.h"
#include "Check.h"
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
constexpr auto kStream = SubjectResidency::Stream::Uv1;
constexpr std::array<uint32_t, 4> kInitial{1, 2, 3, 4};
constexpr std::array<uint32_t, 8> kLarger{5, 6, 7, 8, 9, 10, 11, 12};

struct Faults final : NativeDeviceProxy {
  enum class Point { None, Begin, Map, Submit, MapBusy };

  explicit Faults(Device &device) : NativeDeviceProxy(device) {}

  Point Next = Point::None;

  [[nodiscard]] Result<std::unique_ptr<Commands>> Begin() override {
    if (Next == Point::Begin) {
      Next = Point::None;
      return std::unexpected(
          Error{.Kind = ErrorKind::Busy, .Message = "injected residency admission"});
    }
    return Backing().Begin();
  }

  [[nodiscard]] Result<std::span<std::byte>> MapWrite(Buffer &buffer, ByteRange range) override {
    if (Next == Point::Map || Next == Point::MapBusy) {
      const auto kind = Next == Point::MapBusy ? ErrorKind::Busy : ErrorKind::Driver;
      Next = Point::None;
      return std::unexpected(Error{.Kind = kind, .Message = "injected residency map"});
    }
    return Backing().MapWrite(buffer, range);
  }

  [[nodiscard]] Result<Completion> Submit(std::unique_ptr<Commands> commands) override {
    if (Next == Point::Submit) {
      Next = Point::None;
      return std::unexpected(
          Error{.Kind = ErrorKind::Driver, .Message = "injected residency submission"});
    }
    return Backing().Submit(std::move(commands));
  }
};

Result<void> Cross(SubjectResidency &resident, std::span<const uint32_t> words, bool deferred) {
  SubjectResidency::Crossing crossing{.Which = kStream,
                                      .Usage = BufferUse::Vertex,
                                      .From = words.data(),
                                      .Bytes = static_cast<uint32_t>(words.size_bytes())};
  return resident.Cross(std::span(&crossing, 1), deferred);
}

std::vector<uint32_t> Read(Device &device, SubjectResidency &resident, size_t words) {
  auto transfer = Require(RetryOffline(
      [&] {
        return ReadbackTransfer::Start(
            device, *resident.Buffer(kStream), {.Bytes = words * sizeof(uint32_t)});
      },
      std::chrono::seconds(5)));
  const auto bytes = Require(transfer.Wait(device, std::chrono::seconds(5)));
  CHECK(bytes, "the actual UV1 storage is readable within the explicit offline budget");
  if (!bytes) { Failed(); }
  std::vector<uint32_t> result(words);
  std::memcpy(result.data(), bytes->data(), words * sizeof(uint32_t));
  return result;
}

void FailedCandidates(Faults &device, SubjectResidency &resident) {
  auto *const original = resident.Buffer(kStream).get();
  const auto capacity = resident.HeldOf(kStream);
  for (auto point : {Faults::Point::Begin, Faults::Point::Map, Faults::Point::Submit}) {
    device.Next = point;
    const auto failed = Cross(resident, kLarger, false);
    CHECK(!failed && device.Next == Faults::Point::None,
          "admission, mapping and submission failures reach the caller");
    CHECK(resident.Buffer(kStream).get() == original && resident.HeldOf(kStream) == capacity,
          "a rejected larger candidate restores the previous buffer and capacity");
    CHECK(Read(device, resident, kInitial.size()) ==
              std::vector<uint32_t>(kInitial.begin(), kInitial.end()),
          "failed native uploads leave the previously accepted UV1 bytes unchanged");
  }
  device.Next = Faults::Point::MapBusy;
  const auto allocations = resident.TakeStagingAllocationAttempts();
  static_cast<void>(allocations);
  Require(Cross(resident, kLarger, false));
  CHECK(resident.TakeStagingAllocationAttempts() > 0,
        "a busy upload allocation is replaced instead of blocking the frame");
  CHECK(Read(device, resident, kLarger.size()) ==
            std::vector<uint32_t>(kLarger.begin(), kLarger.end()),
        "cycling upload storage still publishes every source byte");
}

void DeferredRollback(Faults &device, SubjectResidency &resident) {
  Require(Cross(resident, kInitial, true));
  {
    auto commands = Require(device.Begin());
    Require(resident.FlushCrossings(commands.get()));
  }
  CHECK(Read(device, resident, kLarger.size()) ==
            std::vector<uint32_t>(kLarger.begin(), kLarger.end()),
        "abandoned copies cannot modify the accepted buffer");
  const auto scratch = resident.Allocations().TransferBytes;
  resident.ReleaseUploadScratch();
  CHECK(resident.Allocations().TransferBytes == scratch,
        "unaccepted deferred copies retain every staging allocation for retry");
  Require(resident.SubmitPendingUploads());
  CHECK(Read(device, resident, kInitial.size()) ==
            std::vector<uint32_t>(kInitial.begin(), kInitial.end()),
        "an abandoned deferred submission can be retried without source regeneration");
}

void Growth(Device &device, SubjectResidency &resident) {
  auto *const previous = resident.Buffer(kStream).get();
  const auto capacity = resident.HeldOf(kStream);
  Require(
      resident.Grow(kStream, {.Usage = BufferUse::Vertex | BufferUse::Storage, .Bytes = capacity}));
  CHECK(resident.Buffer(kStream).get() != previous,
        "adding a required native usage creates a suitable buffer even at equal capacity");
  CHECK(Read(device, resident, kInitial.size()) ==
            std::vector<uint32_t>(kInitial.begin(), kInitial.end()),
        "native role changes preserve the accepted content");
  Require(resident.Grow(kStream, {.Usage = BufferUse::Storage, .Bytes = capacity * 2}));
  CHECK(Read(device, resident, kInitial.size()) ==
            std::vector<uint32_t>(kInitial.begin(), kInitial.end()),
        "growing an existing stream preserves its occupied prefix");
}
}

int main() {
  auto actual = Require(CreateDevice({.Validation = true}));
  Faults device(*actual);
  SubjectResidency resident;
  Require(resident.StandsOn(&device));
  Require(Cross(resident, kInitial, false));
  FailedCandidates(device, resident);
  DeferredRollback(device, resident);
  Growth(device, resident);
  auto other = Require(CreateDevice({.Validation = true}));
  const auto changed = resident.StandsOn(other.get());
  CHECK(!changed && changed.error().Kind == ErrorKind::Invalid && resident.Device() == &device,
        "live residency cannot silently switch native device ownership");
  return Report();
}
