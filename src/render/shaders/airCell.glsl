#ifndef OUTSHINE_AIR_CELL_GLSL
#define OUTSHINE_AIR_CELL_GLSL
#include "world/sky/AirCellLayout.h"
#define AIR_INLINE
#define AIR_NOEXCEPT
#define AIR_FLOAT(name, value) const float name = value;
#include "world/sky/AirCellConstants.inc"
#undef AIR_FLOAT
#include "world/sky/AirCellCore.h"
#undef AIR_NOEXCEPT
#undef AIR_INLINE
#endif
