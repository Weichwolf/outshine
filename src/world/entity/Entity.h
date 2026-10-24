#ifndef OUTSHINE_WORLD_ENTITY_ENTITY_H
#define OUTSHINE_WORLD_ENTITY_ENTITY_H

#include <cstdint>

namespace outshine {

struct Entity {

  uint32_t Index = 0;

  uint32_t Generation = 0;

  uint64_t Owner = 0;

  [[nodiscard]] constexpr bool operator==(Entity other) const {
    return Index == other.Index && Generation == other.Generation && Owner == other.Owner;
  }
};

inline constexpr Entity kNoEntity{.Index = 0xFFFFFFFFu, .Generation = 0};
}
#endif
