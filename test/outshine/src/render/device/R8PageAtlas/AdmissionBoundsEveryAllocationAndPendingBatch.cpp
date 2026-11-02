#include "AtlasChecks.h"
#include "test/harness/render/NativeDeviceProxy.h"
#include "Check.h"
#include "test/harness/render/NativeGpuCheck.h"
#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <memory>
#include <optional>
#include <span>
#include <string_view>
#include <utility>
#include <vector>

namespace {
using namespace outshine::Render;
using namespace outshine::Render::Native;
using namespace outshine::Test;
using namespace outshine::Test::Gpu;

class Observed final : public NativeDeviceProxy {
public:
  using NativeDeviceProxy::NativeDeviceProxy;
  bool Pending = false;
  size_t Allocations = 0, Waits = 0;

  Result<std::unique_ptr<Buffer>> CreateBuffer(BufferDescription description,
                                               std::string_view label) override {
    ++Allocations;
    return Backing().CreateBuffer(description, label);
  }

  Result<std::unique_ptr<Texture>> CreateTexture(TextureDescription description,
                                                 std::string_view label) override {
    ++Allocations;
    return Backing().CreateTexture(description, label);
  }

  Result<CompletionStatus> Poll(Completion completion) override {
    return Pending ? Result<CompletionStatus>{CompletionStatus::Pending}
                   : Backing().Poll(completion);
  }

  Result<CompletionStatus> Wait(Completion completion, std::chrono::nanoseconds budget) override {
    ++Waits;
    return Backing().Wait(completion, budget);
  }
};

void Admission(Observed &device) {
  const auto options = AtlasOptions();
  const auto storage = Require(R8PageAtlasStorageFor(options));
  for (const auto limits : {R8PageAtlasStorage{.ResidentBytes = storage.ResidentBytes - 1,
                                               .TransferBytes = storage.TransferBytes,
                                               .CpuBytes = storage.CpuBytes},
                            R8PageAtlasStorage{.ResidentBytes = storage.ResidentBytes,
                                               .TransferBytes = storage.TransferBytes - 1,
                                               .CpuBytes = storage.CpuBytes},
                            R8PageAtlasStorage{.ResidentBytes = storage.ResidentBytes,
                                               .TransferBytes = storage.TransferBytes,
                                               .CpuBytes = storage.CpuBytes - 1}}) {
    const auto before = device.Allocations;
    CHECK(!R8PageAtlas::Create(device, options, limits) && device.Allocations == before,
          "resident/transient/CPU admission fails before any native resource allocation");
  }
  auto atlas = Require(R8PageAtlas::Create(device, options, storage));
  DrainAtlas(device, atlas);
  std::vector<uint8_t> pixels(size_t{66} * 66, 73);
  constexpr FieldPageAddress key{.X = -1, .Y = 0, .Mip = 1};
  const std::array valid{R8PageEdit{.Slot = 0, .Address = key, .Pixels = pixels}};
  for (const auto edit :
       {R8PageEdit{.Slot = 3, .Address = key, .Pixels = pixels},
        R8PageEdit{.Slot = 0, .Address = key, .Pixels = std::span(pixels).first(3)},
        R8PageEdit{.Slot = 0, .Address = std::nullopt, .Pixels = pixels}}) {
    const auto before = device.Allocations;
    CHECK(!atlas.Update(std::span(&edit, 1)) && device.Allocations == before && !atlas.Find(key),
          "invalid slot/body/retirement edits allocate no upload buffers and publish nothing");
  }
  std::array duplicates{valid[0], valid[0]};
  CHECK(!atlas.Update(duplicates),
        "one slot cannot receive conflicting edits within a transaction");
  device.Pending = true;
  const auto waits = device.Waits;
  CHECK(Require(atlas.Update(valid)) && Require(atlas.Update(valid)),
        "the explicit pending batch limit admits exactly two uploads");
  const auto before = device.Allocations;
  const auto rejected = atlas.Update(valid);
  CHECK(!rejected && rejected.error().Kind == ErrorKind::Busy && device.Allocations == before &&
            device.Waits == waits,
        "exhausted GPU retirement credits reject before allocation without a CPU wait");
  device.Pending = false;
  DrainAtlas(device, atlas);
  CHECK(Require(atlas.Update(valid)), "completion releases credits for later updates");
  DrainAtlas(device, atlas);
}
}

int main() {
  auto backend = Require(CreateDevice({.Validation = true}));
  Observed device(*backend);
  Admission(device);
  return outshine::Test::Report();
}
