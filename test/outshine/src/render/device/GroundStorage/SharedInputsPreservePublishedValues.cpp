#include "GroundStorage.h"
#include "Check.h"
#include "ReadbackTransfer.h"
#include "test/harness/render/NativeGpuCheck.h"
#include <array>
#include <bit>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <vector>

namespace {
using namespace outshine::Render;
using namespace outshine::Render::Native;
using namespace outshine::Test;
using namespace outshine::Test::Gpu;

std::vector<uint32_t> Read(Device &device, const Buffer &source, size_t words) {
  auto transfer =
      Require(ReadbackTransfer::Start(device, source, {.Bytes = words * sizeof(uint32_t)}));
  auto bytes = Require(transfer.Wait(device, std::chrono::seconds(5)));
  CHECK(bytes.has_value(), "shared storage reaches the GPU readback timeline");
  if (!bytes) { Failed(); }
  std::vector<uint32_t> result(words);
  std::memcpy(result.data(), bytes->data(), words * sizeof(uint32_t));
  return result;
}
}

int main() {
  auto device = Require(CreateDevice({.Validation = true}));
  const std::array<uint32_t, 4> oldClasses{11, 23, 37, 41};
  const std::array<float, 4> oldPalette{0.125f, 0.25f, 0.5f, 1.0f};
  const auto oldPaletteBits = std::bit_cast<std::array<uint32_t, 4>>(oldPalette);
  GroundStorage kept;
  {
    GroundStorage published;
    Require(published.Replace(device.get(), oldClasses, oldPalette, std::chrono::seconds(5)));
    Require(kept.ReuseInputsFrom(published, *device));
    CHECK(kept.Classes() == published.Classes() && kept.Palette() == published.Palette(),
          "replacement references the actual published immutable buffers");
  }
  CHECK(kept.Ready(), "the published owner may disappear before the replacement reads");
  CHECK(Read(*device, *kept.Classes(), oldClasses.size()) ==
            std::vector<uint32_t>(oldClasses.begin(), oldClasses.end()),
        "class values survive destruction of the original owner");
  CHECK(Read(*device, *kept.Palette(), oldPalette.size()) ==
            std::vector<uint32_t>(oldPaletteBits.begin(), oldPaletteBits.end()),
        "palette values survive destruction of the original owner");
  GroundStorage changed;
  Require(changed.ReuseInputsFrom(kept, *device));
  const std::array<uint32_t, 4> newClasses{47, 53, 61, 71};
  const std::array<float, 4> newPalette{1.0f, 0.5f, 0.25f, 0.125f};
  const auto newPaletteBits = std::bit_cast<std::array<uint32_t, 4>>(newPalette);
  {
    GroundStorage abandoned;
    Require(abandoned.ReuseInputsFrom(kept, *device));
    Require(abandoned.BeginReplace(device.get(), newClasses, newPalette));
    Require(abandoned.AdvanceReplace(16));
  }
  CHECK(Read(*device, *kept.Classes(), oldClasses.size()) ==
            std::vector<uint32_t>(oldClasses.begin(), oldClasses.end()),
        "abandoning an accepted replacement preserves the shared published classes");
  Require(changed.BeginReplace(device.get(), newClasses, newPalette));
  CHECK(changed.Classes() == kept.Classes() && changed.Palette() == kept.Palette(),
        "pending data preserves the complete previous buffer pair");
  constexpr size_t kMaximumSteps = 16;
  for (size_t step = 0; step < kMaximumSteps && changed.UploadActive(); ++step) {
    auto ready = Require(changed.AdvanceReplace(16));
    if (!ready) {
      CHECK(Read(*device, *kept.Classes(), oldClasses.size()) ==
                std::vector<uint32_t>(oldClasses.begin(), oldClasses.end()),
            "in-flight replacement never overwrites the shared published classes");
    }
  }
  CHECK(!changed.UploadActive(), "bounded replacement reaches its complete buffer pair");
  if (changed.UploadActive()) { return Report(); }
  CHECK(changed.Classes() != kept.Classes() && changed.Palette() != kept.Palette(),
        "completed changes publish a separate immutable pair");
  CHECK(Read(*device, *changed.Classes(), newClasses.size()) ==
            std::vector<uint32_t>(newClasses.begin(), newClasses.end()),
        "the new class product reaches its own GPU buffer");
  CHECK(Read(*device, *changed.Palette(), newPalette.size()) ==
            std::vector<uint32_t>(newPaletteBits.begin(), newPaletteBits.end()),
        "the new palette product reaches its own GPU buffer");
  CHECK(Read(*device, *kept.Palette(), oldPalette.size()) ==
            std::vector<uint32_t>(oldPaletteBits.begin(), oldPaletteBits.end()),
        "the original palette remains exact after publication");
  auto otherDevice = Require(CreateDevice());
  GroundStorage foreign;
  auto refused = foreign.ReuseInputsFrom(kept, *otherDevice);
  CHECK(!refused && refused.error().Kind == ErrorKind::Invalid && !foreign.Ready(),
        "buffers cannot be shared between device contexts");
  return Report();
}
