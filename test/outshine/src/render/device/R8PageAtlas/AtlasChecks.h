#ifndef OUTSHINE_TEST_R8PAGEATLAS_ATLASCHECKS_H
#define OUTSHINE_TEST_R8PAGEATLAS_ATLASCHECKS_H
#include "R8PageAtlas.h"
#include "ReadbackTransfer.h"
#include "test/harness/render/NativeGpuCheck.h"
#include <algorithm>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <span>
#include <vector>

namespace outshine::Test::Gpu {
inline Render::R8PageAtlasOptions AtlasOptions() {
  return {.Layout = {.CoreTexels = 64, .Columns = 2, .Entries = 16, .Probes = 8},
          .Slots = 3,
          .EditsPerBatch = 3,
          .PendingBatches = 2};
}

inline void DrainAtlas(Device &device, Render::R8PageAtlas &atlas) {
  if (const auto last = atlas.LastUpload()) {
    CHECK(Require(device.Wait(*last, std::chrono::seconds(5))) == CompletionStatus::Ready,
          "the test drains only the explicitly submitted atlas batch");
  }
  Require(atlas.Collect());
  CHECK(atlas.PendingBatches() == 0, "completed lookup retirements release their bounded credits");
}

inline std::span<const std::byte> AtlasBytes(Device &device, Render::ReadbackTransfer &copy) {
  auto mapped = Require(copy.Wait(device, std::chrono::seconds(5)));
  CHECK(mapped, "the test readback completes within its deadline");
  if (!mapped) { Failed(); }
  return *mapped;
}

inline Render::ReadbackTransfer
CopySlot(Device &device, const Render::R8PageAtlas &atlas, uint32_t slot) {
  const auto shape = atlas.Options().Layout;
  const uint32_t side = shape.CoreTexels + 2;
  return Require(Render::ReadbackTransfer::Start(device,
                                                 atlas.Image(),
                                                 {.X = (slot % shape.Columns) * side,
                                                  .Y = (slot / shape.Columns) * side,
                                                  .Width = side,
                                                  .Height = side}));
}

inline void SlotMatches(Device &device,
                        Render::ReadbackTransfer &copy,
                        std::span<const uint8_t> expected,
                        uint32_t side) {
  const auto bytes = AtlasBytes(device, copy);
  bool same = true;
  for (uint32_t row = 0; row < side; ++row) {
    same = same && std::ranges::equal(bytes.subspan(copy.RowBytes() * row, side),
                                      std::as_bytes(expected.subspan(size_t{row} * side, side)));
  }
  CHECK(same, "only the admitted page region contains its exact submitted pixel body");
}

inline std::vector<Render::FieldPageEntry> LookupEntries(Device &device,
                                                         const Render::R8PageAtlas &atlas) {
  auto transfer = Require(Render::ReadbackTransfer::Start(
      device, atlas.Lookup(), {.Bytes = atlas.Lookup().Description().Bytes}));
  const auto bytes = AtlasBytes(device, transfer);
  Render::FieldPageLayout layout;
  std::memcpy(&layout, bytes.data(), sizeof(layout));
  const auto expected = atlas.Options().Layout;
  CHECK(layout.CoreTexels == expected.CoreTexels && layout.Columns == expected.Columns &&
            layout.Entries == expected.Entries && layout.Probes == expected.Probes,
        "the native lookup header preserves every shader ABI parameter");
  std::vector<Render::FieldPageEntry> table(layout.Entries);
  std::memcpy(table.data(), bytes.data() + sizeof(layout), table.size() * sizeof(table[0]));
  return table;
}
}
#endif
