#ifndef OUTSHINE_WORLD_ENTITY_H
#define OUTSHINE_WORLD_ENTITY_H

#include <cstdint>

namespace outshine {
/// Transient identity in one registry epoch; never a persistent save-file identifier.
/// Registry-issued values distinguish foreign owners, slot reuse and successful reopen.
/// Copies do not own or extend entity lifetime; validate through the originating registry.
struct Entity {
  /// Slot index within the owner; not independently sufficient to identify an entity.
  uint32_t Index = 0;
  /// Slot generation, advanced on removal; exhausted generations retire their slot.
  uint32_t Generation = 0;
  /// Process-unique registry epoch; zero is never issued by an opened registry.
  uint64_t Owner = 0;

  /// @param other Handle to compare; no registry access or lifetime extension.
  /// @return True when owner, slot and generation all match.
  [[nodiscard]] constexpr bool operator==(Entity other) const {
    return Index == other.Index && Generation == other.Generation && Owner == other.Owner;
  }
};

/// Sentinel that no registry can issue as a live entity.
inline constexpr Entity kNoEntity{.Index = 0xFFFFFFFFu, .Generation = 0};
}
#endif
