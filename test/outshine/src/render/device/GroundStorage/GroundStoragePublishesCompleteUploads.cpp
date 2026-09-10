#include <SDL3/SDL.h>
#include <array>
#include <bit>
#include <cstring>
#include <memory>
#include <span>
#include <sys/mman.h>
#include <unistd.h>
#include <vector>
#include "Check.h"
#include "GroundStorage.h"
#include "Readback.h"

namespace {
using namespace outshine::Render;
using namespace outshine::Test;

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

  std::span<const T> Span() const {
    return Value_ ? std::span<const T>(Value_, 1) : std::span<const T>{};
  }

private:
  size_t Page_ = 0;
  void *Memory_ = MAP_FAILED;
  T *Value_ = nullptr;
};

struct Faults {
  enum class Point { None, Map, Acquire, Submit };
  Point Next = Point::None;
  size_t Acquired = 0;
  size_t Submitted = 0;

  GpuSubmission Functions() {
    return {
        .Context = this,
        .Acquire = [](void *context, SDL_GPUDevice *device) -> SDL_GPUCommandBuffer * {
          auto &self = *static_cast<Faults *>(context);
          if (self.Next == Point::Acquire) {
            self.Next = Point::None;
            SDL_SetError("injected ground acquire failure");
            return nullptr;
          }
          ++self.Acquired;
          return SDL_AcquireGPUCommandBuffer(device);
        },
        .Submit = [](void *context, SDL_GPUCommandBuffer *commands) -> SDL_GPUFence * {
          auto &self = *static_cast<Faults *>(context);
          ++self.Submitted;
          if (self.Next == Point::Submit) {
            self.Next = Point::None;
            CHECK(SDL_CancelGPUCommandBuffer(commands), "the failed submit consumes its commands");
            SDL_SetError("injected ground submit failure");
            return nullptr;
          }
          return SDL_SubmitGPUCommandBufferAndAcquireFence(commands);
        },
        .MapUpload =
            [](void *context, SDL_GPUDevice *device, SDL_GPUTransferBuffer *transfer) -> void * {
          auto &self = *static_cast<Faults *>(context);
          if (self.Next == Point::Map) {
            self.Next = Point::None;
            SDL_SetError("injected ground map failure");
            return nullptr;
          }
          return SDL_MapGPUTransferBuffer(device, transfer, false);
        }};
  }
};

std::vector<uint32_t> Read(SDL_GPUDevice *device, SDL_GPUBuffer *buffer, size_t words) {
  Readback reading;
  const auto state =
      reading.FromBuffer(device, buffer, static_cast<uint32_t>(words * sizeof(uint32_t)));
  CHECK(state == ReadState::Ready,
        "the submitted storage contents can be read on the GPU timeline");
  if (state != ReadState::Ready) { return {}; }
  std::vector<uint32_t> result(words);
  std::memcpy(result.data(), reading.Rows(), words * sizeof(uint32_t));
  return result;
}

void Exercise(SDL_GPUDevice *device) {
  GroundStorage storage;
  Faults faults;
  const auto calls = faults.Functions();
  CHECK(!storage.Replace(nullptr, {}, {}) && !storage.Ready(),
        "missing device cannot publish storage");
  CHECK(storage.Replace(device, {}, {}, calls).has_value() && storage.Ready(),
        "empty inputs publish initialized fallback buffers");
  const std::vector<uint32_t> zero(4);
  CHECK(Read(device, storage.Classes(), 4) == zero && Read(device, storage.Palette(), 4) == zero,
        "all minimum-capacity fallback bytes are zero");
  GuardedValue<uint32_t> oneClass(7);
  GuardedValue<float> oneColour(0.25f);
  CHECK(storage.Replace(device, oneClass.Span(), oneColour.Span(), calls).has_value(),
        "single-element inputs never read the inaccessible source tail");
  const std::vector<uint32_t> classes{7, 0, 0, 0};
  const std::vector<uint32_t> palette{std::bit_cast<uint32_t>(0.25f), 0, 0, 0};
  CHECK(Read(device, storage.Classes(), 4) == classes &&
            Read(device, storage.Palette(), 4) == palette,
        "payload and separately initialized padding reach both GPU buffers");
  const auto oldClasses = storage.Classes();
  const auto oldPalette = storage.Palette();
  const std::array<uint32_t, 8> largerClasses{1, 2, 3, 4, 5, 6, 7, 8};
  const std::array<float, 5> largerPalette{1, 0.5f, 0.25f, 0.125f, 0.0625f};
  for (auto point : {Faults::Point::Map, Faults::Point::Acquire, Faults::Point::Submit}) {
    for (int attempt = 0; attempt != 2; ++attempt) {
      faults.Next = point;
      const auto acquired = faults.Acquired;
      const auto submitted = faults.Submitted;
      const auto failed = storage.Replace(device, largerClasses, largerPalette, calls);
      CHECK(!failed && failed.error().starts_with("injected ground"),
            "the original SDL error reaches the caller");
      CHECK(faults.Acquired == acquired + (point == Faults::Point::Submit ? 1u : 0u) &&
                faults.Submitted == submitted + (point == Faults::Point::Submit ? 1u : 0u),
            "no commands are acquired after map failure or submitted after acquire failure");
      CHECK(storage.Classes() == oldClasses && storage.Palette() == oldPalette,
            "failed preparation or submission preserves both published handles");
      CHECK(Read(device, storage.Classes(), 4) == classes &&
                Read(device, storage.Palette(), 4) == palette,
            "both previously submitted payloads survive a failed replacement");
    }
  }
  CHECK(storage.Replace(device, largerClasses, largerPalette, calls).has_value(),
        "a complete retry succeeds");
  CHECK(Read(device, storage.Classes(), largerClasses.size()) ==
            std::vector<uint32_t>(largerClasses.begin(), largerClasses.end()),
        "the larger class payload is complete");
  std::vector<uint32_t> expectedPalette;
  for (float value : largerPalette) { expectedPalette.push_back(std::bit_cast<uint32_t>(value)); }
  CHECK(Read(device, storage.Palette(), largerPalette.size()) == expectedPalette,
        "the larger palette is complete");
  CHECK(storage.Replace(device, {}, {}, calls).has_value(),
        "empty replacement clears a previously populated pair");
  CHECK(Read(device, storage.Classes(), 4) == zero && Read(device, storage.Palette(), 4) == zero,
        "cleared storage cannot expose stale palette or class data");
  CHECK(faults.Acquired == faults.Submitted, "every acquired upload command is consumed once");
}
}

int main() {
  CHECK(SDL_SetHint(SDL_HINT_ASSERT, "abort"), "SDL assertions fail without a dialog");
  CHECK(SDL_Init(SDL_INIT_VIDEO), "SDL video initializes");
#ifdef OUTSHINE_GPU_VALIDATION
  constexpr bool debug = true;
#else
  constexpr bool debug = false;
#endif
  {
    OwnedDevice device(SDL_CreateGPUDevice(SDL_GPU_SHADERFORMAT_MSL | SDL_GPU_SHADERFORMAT_SPIRV |
                                               SDL_GPU_SHADERFORMAT_DXIL,
                                           debug,
                                           nullptr));
    CHECK(device.Get() != nullptr, "a real GPU device is available");
    if (device) { Exercise(device.Get()); }
  }
  SDL_Quit();
  Covers("bounded GPU uploads, initialized padding, atomic class/palette publication, "
         "map/acquire/submit failures and retry");
  return Report();
}
