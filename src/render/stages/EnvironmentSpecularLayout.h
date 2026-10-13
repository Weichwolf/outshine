#ifndef OUTSHINE_RENDER_STAGES_ENVIRONMENTSPECULARLAYOUT_H
#define OUTSHINE_RENDER_STAGES_ENVIRONMENTSPECULARLAYOUT_H

#include <cstdint>

namespace outshine::Render {
#define ENV_INT(name, value) inline constexpr uint32_t name = value;
#include "EnvironmentSpecularValues.inc"
#undef ENV_INT
inline constexpr uint32_t kEnvironmentWidth = kEnvironmentSide + 2u * kEnvironmentBorder;
inline constexpr uint32_t kEnvironmentHeight = kEnvironmentWidth * kEnvironmentLevels;
static_assert(kEnvironmentDiffuseLevel == kEnvironmentSpecularLevels);
static_assert(kEnvironmentLevels == kEnvironmentDiffuseLevel + 1u);
}

#endif
