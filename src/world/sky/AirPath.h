#ifndef OUTSHINE_WORLD_SKY_AIRPATH_H
#define OUTSHINE_WORLD_SKY_AIRPATH_H

#include "Atmosphere.h"
#include <algorithm>
#include <cmath>

namespace outshine {
#include "AirRayLayout.h"
#include "AirPathLayout.h"

namespace air_path {
using std::clamp;
using std::max;
using std::min;
using std::sqrt;
#define AIR_INLINE [[nodiscard]] inline
#define AIR_NOEXCEPT noexcept
#define MEDIUM_ARG const Medium &
#include "AirPathCore.h"
#undef MEDIUM_ARG
#undef AIR_NOEXCEPT
#undef AIR_INLINE
}
}

#endif
