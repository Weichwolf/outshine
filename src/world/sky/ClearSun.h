#ifndef OUTSHINE_WORLD_SKY_CLEARSUN_H
#define OUTSHINE_WORLD_SKY_CLEARSUN_H

#include "Atmosphere.h"

namespace outshine::medium_core {
#define MEDIUM_ARG const Medium &
#define MEDIUM_INLINE [[nodiscard]] inline
#define SUN_NOEXCEPT noexcept
#include "ClearSunCore.h"
#undef SUN_NOEXCEPT
#undef MEDIUM_INLINE
#undef MEDIUM_ARG
}

#endif
