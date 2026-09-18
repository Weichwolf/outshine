#ifndef OUTSHINE_RENDER_SCENE_RESOURCEHANDLE_H
#define OUTSHINE_RENDER_SCENE_RESOURCEHANDLE_H

#include <cstdint>
#include <limits>

namespace outshine::Render {
inline constexpr uint32_t kNoResourceSlot = std::numeric_limits<uint32_t>::max();

struct PieceHandle {
  uint32_t Slot = kNoResourceSlot;
  uint64_t Generation = 0;

  [[nodiscard]] explicit constexpr operator bool() const noexcept {
    return Slot != kNoResourceSlot && Generation != 0;
  }

  [[nodiscard]] constexpr bool operator==(const PieceHandle &) const noexcept = default;
};

struct HeightPageHandle {
  uint32_t Slot = kNoResourceSlot;
  uint64_t Generation = 0;

  [[nodiscard]] explicit constexpr operator bool() const noexcept {
    return Slot != kNoResourceSlot && Generation != 0;
  }

  [[nodiscard]] constexpr bool operator==(const HeightPageHandle &) const noexcept = default;
};

struct ResourceSlotState {
  uint64_t Generation = 1;
  uint32_t NextFree = kNoResourceSlot;
  bool Occupied = false;

  [[nodiscard]] constexpr bool Matches(uint64_t generation) const noexcept {
    return Occupied && Generation == generation;
  }

  [[nodiscard]] constexpr bool Release() noexcept {
    if (!Occupied) { return false; }
    Occupied = false;
    if (Generation == std::numeric_limits<uint64_t>::max()) { return false; }
    ++Generation;
    return true;
  }
};
}
#endif
