#ifndef OUTSHINE_GENERATORS_TERRAIN_GROUNDYIELD_H
#define OUTSHINE_GENERATORS_TERRAIN_GROUNDYIELD_H

#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

#include "GroundMesher.h"

namespace outshine {

void DivideOnClass(const GroundMesh &mesh, double finestM, Yielded &told);

void YieldGround(std::span<const Yields> these, Budget within, GroundMesh mesh, Yielded &told);

size_t
PressPoints(std::span<const Yields> these, std::span<const EastSouth> at, std::span<double> upM);

} // namespace outshine
#endif
