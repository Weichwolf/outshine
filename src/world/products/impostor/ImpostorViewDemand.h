#ifndef OUTSHINE_WORLD_PRODUCTS_IMPOSTOR_IMPOSTORVIEWDEMAND_H
#define OUTSHINE_WORLD_PRODUCTS_IMPOSTOR_IMPOSTORVIEWDEMAND_H

#include "Vec3.h"
#include <cstddef>
#include <span>
#include <vector>

namespace outshine {

struct ImpostorEyeSweep {
  Vec3 Begin;
  Vec3 End;
};

[[nodiscard]] std::vector<size_t> SelectImpostorDirections(std::span<const Vec3> directions,
                                                           const ImpostorEyeSweep &eye);

}
#endif
