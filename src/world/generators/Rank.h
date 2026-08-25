#ifndef OUTSHINE_WORLD_GENERATORS_RANK_H
#define OUTSHINE_WORLD_GENERATORS_RANK_H

#include <cstdint>

namespace outshine::Generators {

enum class Rank : uint16_t {};

struct BodyRange {
  uint32_t First = 0, Count = 0;
};

}
#endif
