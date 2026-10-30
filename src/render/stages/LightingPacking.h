#ifndef OUTSHINE_RENDER_STAGES_LIGHTINGPACKING_H
#define OUTSHINE_RENDER_STAGES_LIGHTINGPACKING_H
#include "LightingUniform.h"
#include "SceneLighting.h"
#include "FrameContext.h"
#include <span>

namespace outshine::Render {
[[nodiscard]] LightingUniform PackLightingUniform(std::span<const SceneLight> placed,
                                                  const EnvironmentLighting &environment,
                                                  const FrameContext &frame);
}
#endif
