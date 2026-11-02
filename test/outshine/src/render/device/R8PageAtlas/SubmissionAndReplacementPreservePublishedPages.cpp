#include "AtlasChecks.h"
#include "test/harness/render/NativeGpuCheck.h"
#include "test/harness/render/NativeUploadFaultDevice.h"
#include "Check.h"
#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <utility>
#include <vector>

namespace {
using namespace outshine::Render;
using namespace outshine::Render::Native;
using namespace outshine::Test;
using namespace outshine::Test::Gpu;

void Transactions(NativeUploadFaultDevice &device, UploadFailure &failure) {
  const auto options = AtlasOptions();
  const auto storage = Require(R8PageAtlasStorageFor(options));
  auto atlas = Require(R8PageAtlas::Create(device, options, storage));
  DrainAtlas(device, atlas);
  constexpr FieldPageAddress first{.X = -65, .Y = -64, .Mip = 5};
  constexpr FieldPageAddress second{};
  constexpr FieldPageAddress third{.X = 12345, .Y = -23456, .Mip = 2};
  std::vector<uint8_t> dark(size_t{66} * 66, 17);
  std::vector<uint8_t> light(size_t{66} * 66, 211);
  std::vector<uint8_t> neighbour(size_t{66} * 66, 83);
  const std::array original{R8PageEdit{.Slot = 0, .Address = first, .Pixels = dark},
                            R8PageEdit{.Slot = 2, .Address = second, .Pixels = neighbour}};
  const auto waits = failure.Waits;
  CHECK(Require(atlas.Update(original)), "a bounded native batch publishes two nonadjacent slots");
  CHECK(failure.Waits == waits, "atlas admission and submission never wait on the CPU");
  DrainAtlas(device, atlas);
  auto before = CopySlot(device, atlas, 0);
  const auto *oldLookup = &atlas.Lookup();
  const std::array edit{R8PageEdit{.Slot = 0, .Address = third, .Pixels = light}};
  for (const auto point : {UploadFailurePoint::Acquire,
                           UploadFailurePoint::Map,
                           UploadFailurePoint::Copy,
                           UploadFailurePoint::Submit}) {
    failure.Point = point;
    failure.Kind = ErrorKind::Busy;
    failure.Skip = point == UploadFailurePoint::Copy ? 1 : 0;
    failure.Armed = true;
    const auto rejected = atlas.Update(edit);
    CHECK(!rejected && rejected.error().Kind == ErrorKind::Busy && !failure.Armed,
          "native acquire/map/partial-copy/submission rejection remains retryable");
    CHECK(&atlas.Lookup() == oldLookup && atlas.Find(first) == 0 && !atlas.Find(third),
          "rejection preserves the complete published lookup and CPU residency");
    auto unchanged = CopySlot(device, atlas, 0);
    SlotMatches(device, unchanged, dark, 66);
  }
  CHECK(Require(atlas.Update(edit)), "the same prepared body succeeds after native backpressure");
  CHECK(!atlas.Find(first) && atlas.Find(third) == 0 && atlas.Find(second) == 2,
        "replacement retires the old key while retaining unrelated resident pages");
  auto after = CopySlot(device, atlas, 0);
  SlotMatches(device, before, dark, 66);
  SlotMatches(device, after, light, 66);
  auto untouched = CopySlot(device, atlas, 2);
  SlotMatches(device, untouched, neighbour, 66);
  const auto table = LookupEntries(device, atlas);
  CHECK(!FindFieldPageSlot(table, options.Layout.Probes, first) &&
            FindFieldPageSlot(table, options.Layout.Probes, third) == 0,
        "GPU lookup bytes publish the replacement without retaining the evicted virtual key");
  const std::array remove{R8PageEdit{.Slot = 0, .Address = std::nullopt, .Pixels = {}}};
  CHECK(Require(atlas.Update(remove)), "a slot can retire without uploading a replacement image");
  const auto removed = LookupEntries(device, atlas);
  CHECK(!atlas.Find(third) && !FindFieldPageSlot(removed, options.Layout.Probes, third),
        "retired physical pixels become unreachable through both lookup consumers");
  DrainAtlas(device, atlas);
}
}

int main() {
  UploadFailure failure;
  NativeUploadFaultDevice device(Require(CreateDevice({.Validation = true})), failure);
  Transactions(device, failure);
  return outshine::Test::Report();
}
