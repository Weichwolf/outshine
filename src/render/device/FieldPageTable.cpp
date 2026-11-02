#include "FieldPageTable.h"
#include <bit>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <limits>
#include <optional>
#include <span>
#include <utility>
#include <vector>

namespace outshine::Render {
uint32_t FieldPageHash(FieldPageAddress address) noexcept {
  constexpr uint32_t x = 0x9e3779b9u;
  constexpr uint32_t y = 0x85ebca6bu;
  constexpr uint32_t z = 0xc2b2ae35u;
  constexpr uint32_t first = 0x7feb352du;
  constexpr uint32_t second = 0x846ca68bu;
  uint32_t h = static_cast<uint32_t>(uint64_t{static_cast<uint32_t>(address.X)} * x) ^
               static_cast<uint32_t>(uint64_t{static_cast<uint32_t>(address.Y)} * y) ^
               static_cast<uint32_t>(uint64_t{address.Mip} * z);
  h = static_cast<uint32_t>(uint64_t{h ^ (h >> 16u)} * first);
  h = static_cast<uint32_t>(uint64_t{h ^ (h >> 15u)} * second);
  return h ^ (h >> 16u);
}

Native::Result<std::vector<FieldPageEntry>> BuildFieldPageTable(
    std::span<const std::optional<FieldPageAddress>> slots, uint32_t entries, uint32_t probes) {
  if (!std::has_single_bit(entries) || probes == 0 || probes > entries || slots.size() > entries ||
      std::cmp_greater_equal(slots.size(), std::numeric_limits<uint32_t>::max())) {
    return std::unexpected(Native::Error{.Kind = Native::ErrorKind::Invalid,
                                         .Message = "invalid bounded field page table shape"});
  }
  std::vector<FieldPageEntry> table(entries);
  for (size_t slot = 0; slot < slots.size(); ++slot) {
    const auto &resident = slots[slot];
    if (!resident) { continue; }
    const auto address = *resident;
    if (address.Mip >= std::numeric_limits<uint32_t>::digits) {
      return std::unexpected(
          Native::Error{.Kind = Native::ErrorKind::Invalid,
                        .Message = "field page mip exceeds shader address range"});
    }
    const uint32_t first = FieldPageHash(address);
    bool placed = false;
    for (uint32_t probe = 0; probe < probes; ++probe) {
      auto &entry = table[(uint64_t{first} + probe) & (entries - 1u)];
      if (entry.SlotPlusOne == 0) {
        entry = {.Address = address, .SlotPlusOne = static_cast<uint32_t>(slot) + 1u};
        placed = true;
        break;
      }
      if (entry.Address == address) {
        return std::unexpected(
            Native::Error{.Kind = Native::ErrorKind::Invalid,
                          .Message = "field page occupies multiple atlas slots"});
      }
    }
    if (!placed) {
      return std::unexpected(
          Native::Error{.Kind = Native::ErrorKind::Unsupported,
                        .Message = "field page table exceeds admitted probe count"});
    }
  }
  return table;
}

std::optional<uint32_t> FindFieldPageSlot(std::span<const FieldPageEntry> table,
                                          uint32_t probes,
                                          FieldPageAddress address) noexcept {
  if (!std::has_single_bit(table.size()) || probes == 0 || probes > table.size()) { return {}; }
  const uint32_t first = FieldPageHash(address);
  for (uint32_t probe = 0; probe < probes; ++probe) {
    const auto &entry = table[(size_t{first} + probe) & (table.size() - 1u)];
    if (entry.SlotPlusOne == 0) { return {}; }
    if (entry.Address == address) { return entry.SlotPlusOne - 1u; }
  }
  return {};
}
}
