#include "Check.h"
#include "GroundStorage.h"
#include "ReadbackTransfer.h"
#include "test/harness/render/NativeGpuCheck.h"
#include "test/harness/render/NativeDeviceProxy.h"
#include <array>
#include <bit>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <expected>
#include <memory>
#include <span>
#include <sys/mman.h>
#include <thread>
#include <unistd.h>
#include <utility>
#include <vector>

namespace {
using namespace outshine::Render;
using namespace outshine::Render::Native;
using namespace outshine::Test;
using namespace outshine::Test::Gpu;
constexpr size_t kPollSteps = 2000;
constexpr float kSingleColour = 0.25f;

template <class T> class GuardedValue {
public:
  explicit GuardedValue(T value) {
    Page_ = static_cast<size_t>(sysconf(_SC_PAGESIZE));
    Memory_ = mmap(nullptr, 2 * Page_, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANON, -1, 0);
    CHECK(Memory_ != MAP_FAILED, "the one-element source has a guarded allocation");
    if (Memory_ == MAP_FAILED) { return; }
    CHECK(mprotect(static_cast<char *>(Memory_) + Page_, Page_, PROT_NONE) == 0,
          "reading beyond the single source element faults");
    Value_ = std::construct_at(
        reinterpret_cast<T *>(static_cast<char *>(Memory_) + Page_ - sizeof(T)), value);
  }

  ~GuardedValue() {
    if (Memory_ != MAP_FAILED) { munmap(Memory_, 2 * Page_); }
  }

  [[nodiscard]] std::span<const T> Span() const {
    return Value_ ? std::span<const T>(Value_, 1) : std::span<const T>{};
  }

private:
  size_t Page_ = 0;
  void *Memory_ = MAP_FAILED;
  T *Value_ = nullptr;
};

struct Faults final : NativeDeviceProxy {
  enum class Point { None, Map, Acquire, Submit, Busy };

  explicit Faults(Device &device) : NativeDeviceProxy(device) {}

  Point Next = Point::None;
  size_t Acquired = 0;
  size_t Submitted = 0;
  size_t MapFailures = 0;
  size_t MapAttempts = 0;

  [[nodiscard]] Result<std::unique_ptr<Commands>> Begin() override {
    if (Next == Point::Acquire || Next == Point::Busy) {
      const auto kind = Next == Point::Busy ? ErrorKind::Busy : ErrorKind::Driver;
      Next = Point::None;
      return std::unexpected(Error{.Kind = kind, .Message = "injected ground acquire failure"});
    }
    auto result = Backing().Begin();
    if (result) { ++Acquired; }
    return result;
  }

  [[nodiscard]] Result<Completion> Submit(std::unique_ptr<Commands> commands) override {
    ++Submitted;
    if (Next == Point::Submit) {
      Next = Point::None;
      return std::unexpected(
          Error{.Kind = ErrorKind::Driver, .Message = "injected ground submit failure"});
    }
    return Backing().Submit(std::move(commands));
  }

  [[nodiscard]] Result<std::span<std::byte>> MapWrite(Buffer &buffer, ByteRange range) override {
    ++MapAttempts;
    if (Next == Point::Map) {
      Next = Point::None;
      ++MapFailures;
      return std::unexpected(
          Error{.Kind = ErrorKind::Driver, .Message = "injected ground map failure"});
    }
    return Backing().MapWrite(buffer, range);
  }
};

std::vector<uint32_t> Read(Device *device, Buffer *buffer, size_t words) {
  auto readback =
      Require(ReadbackTransfer::Start(*device, *buffer, {.Bytes = words * sizeof(uint32_t)}));
  const auto bytes = Require(readback.Wait(*device, std::chrono::seconds(5)));
  CHECK(bytes, "the submitted storage contents can be read on the GPU timeline");
  if (!bytes) { Failed(); }
  std::vector<uint32_t> result(words);
  std::memcpy(result.data(), bytes->data(), words * sizeof(uint32_t));
  return result;
}

void FailedReplacements(Faults *device,
                        GroundStorage &storage,
                        const std::vector<uint32_t> &classes,
                        const std::vector<uint32_t> &palette) {
  auto &faults = *device;
  auto *const oldClasses = storage.Classes();
  auto *const oldPalette = storage.Palette();
  const std::vector<uint32_t> zero(4);
  const std::array<uint32_t, 8> largerClasses{1, 2, 3, 4, 5, 6, 7, 8};
  const std::array<float, 5> largerPalette{1, 0.5f, 0.25f, 0.125f, 0.0625f};
  for (auto point : {Faults::Point::Map, Faults::Point::Acquire, Faults::Point::Submit}) {
    for (int attempt = 0; attempt != 2; ++attempt) {
      faults.Next = point;
      const auto acquired = faults.Acquired;
      const auto submitted = faults.Submitted;
      const auto failed =
          storage.Replace(device, largerClasses, largerPalette, std::chrono::seconds(5));
      CHECK(!failed && failed.error().Message.starts_with("injected ground"),
            "the original native error reaches the caller");
      // Native admission precedes staging so a busy device avoids the host copy.
      // Map failure abandons that recording; neither failure path submits work.
      CHECK(faults.Acquired == acquired + (point == Faults::Point::Acquire ? 0u : 1u) &&
                faults.Submitted == submitted + (point == Faults::Point::Submit ? 1u : 0u),
            "admission precedes staging; map or acquire failure cannot submit commands");
      CHECK(storage.Classes() == oldClasses && storage.Palette() == oldPalette,
            "failed preparation or submission preserves both published handles");
      CHECK(Read(device, storage.Classes(), 4) == classes &&
                Read(device, storage.Palette(), 4) == palette,
            "both previously submitted payloads survive a failed replacement");
    }
  }
  CHECK(storage.Replace(device, largerClasses, largerPalette, std::chrono::seconds(5)).has_value(),
        "a complete retry succeeds");
  CHECK(Read(device, storage.Classes(), largerClasses.size()) ==
            std::vector<uint32_t>(largerClasses.begin(), largerClasses.end()),
        "the larger class payload is complete");
  std::vector<uint32_t> expectedPalette;
  expectedPalette.reserve(largerPalette.size());
  for (const float value : largerPalette) {
    expectedPalette.push_back(std::bit_cast<uint32_t>(value));
  }
  CHECK(Read(device, storage.Palette(), largerPalette.size()) == expectedPalette,
        "the larger palette is complete");
  CHECK(storage.Replace(device, {}, {}, std::chrono::seconds(5)).has_value(),
        "empty replacement clears a previously populated pair");
  CHECK(Read(device, storage.Classes(), 4) == zero && Read(device, storage.Palette(), 4) == zero,
        "cleared storage cannot expose stale palette or class data");
}

void PacedReplacement(Faults *device,
                      GroundStorage &storage,
                      const std::vector<uint32_t> &manyClasses,
                      const std::array<float, 8> &manyPalette) {
  auto &faults = *device;
  const Buffer *const priorClasses = storage.Classes();
  const Buffer *const priorPalette = storage.Palette();
  CHECK(storage.BeginReplace(device, manyClasses, manyPalette).has_value(),
        "bounded class upload starts without publishing either buffer");
  CHECK(!storage.BeginReplace(device, manyClasses, manyPalette),
        "a second transaction cannot replace an unfinished upload");
  faults.Next = Faults::Point::Busy;
  const auto mapsBeforePressure = faults.MapAttempts;
  GroundStorageUploadMetrics pressure;
  const auto deferred = storage.AdvanceReplace(32, &pressure);
  CHECK(deferred && !*deferred && storage.UploadActive() && pressure.BytesSubmitted == 0 &&
            faults.MapAttempts == mapsBeforePressure && storage.Classes() == priorClasses &&
            storage.Palette() == priorPalette,
        "native back pressure defers the upload before staging and preserves the committed pair");
  bool complete = false;
  size_t rangeCount = 0;
  for (size_t step = 0; step < kPollSteps && !complete; ++step) {
    GroundStorageUploadMetrics metrics;
    const auto advanced = storage.AdvanceReplace(32, &metrics);
    CHECK(advanced.has_value(),
          advanced ? "class range advances" : advanced.error().Message.c_str());
    if (!advanced) { break; }
    CHECK(metrics.BytesSubmitted <= 32 && metrics.BytesSubmitted % sizeof(uint32_t) == 0,
          "one advance submits at most its aligned byte budget");
    rangeCount += metrics.BytesSubmitted != 0 ? 1u : 0u;
    complete = *advanced;
    if (!complete) {
      CHECK(storage.Classes() == priorClasses && storage.Palette() == priorPalette,
            "no incomplete class or palette range becomes visible");
      std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
  }
  CHECK(complete && rangeCount > 2 &&
            Read(device, storage.Classes(), manyClasses.size()) == manyClasses,
        "multiple interrupted ranges publish exact class words after the final fence");
  std::vector<uint32_t> manyPaletteWords;
  manyPaletteWords.reserve(manyPalette.size());
  for (const float value : manyPalette) {
    manyPaletteWords.push_back(std::bit_cast<uint32_t>(value));
  }
  CHECK(Read(device, storage.Palette(), manyPalette.size()) == manyPaletteWords,
        "paced palette ranges retain their exact values");
  GroundStorage synchronous;
  CHECK(
      synchronous.Replace(device, manyClasses, manyPalette, std::chrono::seconds(5)).has_value() &&
          Read(device, synchronous.Classes(), manyClasses.size()) ==
              Read(device, storage.Classes(), manyClasses.size()) &&
          Read(device, synchronous.Palette(), manyPalette.size()) ==
              Read(device, storage.Palette(), manyPalette.size()),
      "synchronous and paced transactions publish the same GPU bytes");
}

void InterruptReplacement(Faults *device,
                          GroundStorage &storage,
                          const std::vector<uint32_t> &manyClasses,
                          const std::array<float, 8> &manyPalette) {
  auto &faults = *device;
  const Buffer *const publishedClasses = storage.Classes();
  const Buffer *const publishedPalette = storage.Palette();
  CHECK(storage.BeginReplace(device, manyClasses, manyPalette).has_value(),
        "a second bounded transaction begins");
  const auto firstRange = storage.AdvanceReplace(32);
  CHECK(firstRange.has_value() && !*firstRange, "a submitted range leaves the replacement private");
  faults.Next = Faults::Point::Map;
  bool failedAfterRange = false;
  for (size_t step = 0; step < kPollSteps && !failedAfterRange; ++step) {
    const auto advanced = storage.AdvanceReplace(32);
    failedAfterRange = !advanced;
    if (!failedAfterRange) { std::this_thread::sleep_for(std::chrono::milliseconds(1)); }
  }
  CHECK(failedAfterRange && !storage.UploadActive() && storage.Classes() == publishedClasses &&
            storage.Palette() == publishedPalette &&
            Read(device, storage.Classes(), manyClasses.size()) == manyClasses,
        "a failure after one submitted range preserves the last complete pair");
  {
    GroundStorage canceled;
    CHECK(canceled.BeginReplace(device, manyClasses, manyPalette).has_value(),
          "a disposable candidate starts its own upload");
    const auto submitted = canceled.AdvanceReplace(32);
    CHECK(submitted.has_value() && !*submitted, "the disposable candidate owns an in-flight range");
  }
  auto afterCancel = Require(device->Begin());
  const auto completion = Require(device->Submit(std::move(afterCancel)));
  CHECK(Require(device->Wait(completion, std::chrono::seconds(5))) == CompletionStatus::Ready,
        "releasing an in-flight candidate leaves the actual device usable");
  CHECK(Read(device, storage.Classes(), manyClasses.size()) == manyClasses,
        "canceling an in-flight upload preserves the committed world");
  CHECK(faults.Acquired == faults.Submitted + faults.MapFailures,
        "every acquired recording is submitted or abandoned after an injected map failure");
}

void Exercise(Faults *device) {
  GroundStorage storage;
  CHECK(!storage.Replace(nullptr, {}, {}, std::chrono::seconds(5)) && !storage.Ready(),
        "missing device cannot publish storage");
  CHECK(storage.Replace(device, {}, {}, std::chrono::seconds(5)).has_value() && storage.Ready(),
        "empty inputs publish initialized fallback buffers");
  const std::vector<uint32_t> zero(4);
  CHECK(Read(device, storage.Classes(), 4) == zero && Read(device, storage.Palette(), 4) == zero,
        "all minimum-capacity fallback bytes are zero");
  const GuardedValue<uint32_t> oneClass(7);
  const GuardedValue<float> oneColour(kSingleColour);
  CHECK(storage.Replace(device, oneClass.Span(), oneColour.Span(), std::chrono::seconds(5))
            .has_value(),
        "single-element inputs never read the inaccessible source tail");
  const std::vector<uint32_t> classes{7, 0, 0, 0};
  const std::vector<uint32_t> palette{std::bit_cast<uint32_t>(0.25f), 0, 0, 0};
  CHECK(Read(device, storage.Classes(), 4) == classes &&
            Read(device, storage.Palette(), 4) == palette,
        "payload and separately initialized padding reach both GPU buffers");
  auto *const oldClasses = storage.Classes();
  auto *const oldPalette = storage.Palette();
  const auto expired =
      storage.Replace(device, oneClass.Span(), oneColour.Span(), std::chrono::nanoseconds(1));
  CHECK(!expired && expired.error().Kind == ErrorKind::Busy && !storage.UploadActive() &&
            storage.Classes() == oldClasses && storage.Palette() == oldPalette,
        "an expired explicit offline budget cancels the private candidate and preserves both "
        "handles");
  FailedReplacements(device, storage, classes, palette);
  std::vector<uint32_t> manyClasses(64);
  for (size_t at = 0; at < manyClasses.size(); ++at) {
    manyClasses[at] = static_cast<uint32_t>(at + 1u);
  }
  const std::array<float, 8> manyPalette{0, 0.125f, 0.25f, 0.375f, 0.5f, 0.625f, 0.75f, 0.875f};
  PacedReplacement(device, storage, manyClasses, manyPalette);
  InterruptReplacement(device, storage, manyClasses, manyPalette);
}
}

int main() {
  auto device = Require(CreateDevice({.Validation = true}));
  Faults forwarding(*device);
  Exercise(&forwarding);
  Covers("bounded native GPU uploads, initialized padding, atomic class/palette publication, "
         "map/acquire/submit failures and retry");
  return Report();
}
