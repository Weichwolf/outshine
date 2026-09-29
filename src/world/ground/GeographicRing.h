#ifndef OUTSHINE_WORLD_GROUND_GEOGRAPHICRING_H
#define OUTSHINE_WORLD_GROUND_GEOGRAPHICRING_H

#include <cstdint>

namespace outshine {
struct GeographicRing {
  uint32_t First = 0, Count = 0;
  bool Exterior = true;
};
}
#endif
