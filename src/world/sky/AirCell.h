#ifndef OUTSHINE_WORLD_SKY_AIRCELL_H
#define OUTSHINE_WORLD_SKY_AIRCELL_H

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <type_traits>

namespace outshine {

#include "AirCellLayout.h"

static_assert(std::is_trivially_copyable_v<AirCellInput> && sizeof(AirCellInput) == 32);
static_assert(offsetof(AirCellInput, WidthKm) == 0 && offsetof(AirCellInput, DistanceKm) == 4 &&
              offsetof(AirCellInput, ExtinctionA) == 8 &&
              offsetof(AirCellInput, SourceA) == sizeof(AirCellInput) - 3 * sizeof(float) &&
              offsetof(AirCellInput, SourceC) == sizeof(AirCellInput) - sizeof(float));
static_assert(std::is_trivially_copyable_v<AirCellChannel> && sizeof(AirCellChannel) == 8 &&
              offsetof(AirCellChannel, Transmittance) == 4);

namespace air_cell {
using std::exp;
using std::max;
#define AIR_INLINE [[nodiscard]] inline
#define AIR_NOEXCEPT noexcept
#define AIR_FLOAT(name, value) inline constexpr float name = value;
#include "AirCellConstants.inc"
#undef AIR_FLOAT
#include "AirCellCore.h"
#undef AIR_NOEXCEPT
#undef AIR_INLINE
}

}
#endif
