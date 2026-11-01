#ifndef OUTSHINE_RENDER_STAGES_LIGHTINGPACKING_H
#define OUTSHINE_RENDER_STAGES_LIGHTINGPACKING_H
#include "LightingUniform.h"
#include "SceneLighting.h"
#include "FrameContext.h"
#include <span>

namespace outshine::Render {
struct EnvironmentMapFrame {
  Vec3f Up = {{0, 1, 0}};
  Vec3f ToSun = {{0, 1, 0}};
};

[[nodiscard]] LightingUniform PackLightingUniform(std::span<const SceneLight> placed,
                                                  const EnvironmentLighting &environment,
                                                  const FrameContext &frame);
void PackEnvironmentMap(LightingUniform &into, double skyLux, const EnvironmentMapFrame &frame);
}
#endif
