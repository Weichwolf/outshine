#ifndef OUTSHINE_RENDER_DEVICE_FIELDPAGETABLE_H
#define OUTSHINE_RENDER_DEVICE_FIELDPAGETABLE_H

#include "native/Commands.h"
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <vector>

namespace outshine::Render {
struct FieldPageAddress {
  int32_t X = 0;
  int32_t Y = 0;
  uint32_t Mip = 0;
  bool operator==(const FieldPageAddress &) const = default;
};

struct alignas(16) FieldPageEntry {
  FieldPageAddress Address;
  uint32_t SlotPlusOne = 0;
};

struct alignas(16) FieldPageLayout {
  uint32_t CoreTexels = 0;
  uint32_t Columns = 0;
  uint32_t Entries = 0;
  uint32_t Probes = 0;
};

static_assert(sizeof(FieldPageAddress) == 12 && alignof(FieldPageAddress) == 4 &&
              offsetof(FieldPageAddress, X) == 0 && offsetof(FieldPageAddress, Y) == 4 &&
              offsetof(FieldPageAddress, Mip) == 8);
static_assert(sizeof(FieldPageEntry) == 16 && alignof(FieldPageEntry) == 16 &&
              offsetof(FieldPageEntry, Address) == 0 &&
              offsetof(FieldPageEntry, SlotPlusOne) == 12);
static_assert(sizeof(FieldPageLayout) == 16 && alignof(FieldPageLayout) == 16 &&
              offsetof(FieldPageLayout, CoreTexels) == 0 &&
              offsetof(FieldPageLayout, Columns) == 4 && offsetof(FieldPageLayout, Entries) == 8 &&
              offsetof(FieldPageLayout, Probes) == 12);

[[nodiscard]] uint32_t FieldPageHash(FieldPageAddress address) noexcept;
[[nodiscard]] Native::Result<std::vector<FieldPageEntry>> BuildFieldPageTable(
    std::span<const std::optional<FieldPageAddress>> slots, uint32_t entries, uint32_t probes);
[[nodiscard]] std::optional<uint32_t> FindFieldPageSlot(std::span<const FieldPageEntry> table,
                                                        uint32_t probes,
                                                        FieldPageAddress address) noexcept;
}
#endif
