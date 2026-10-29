#include "LightingPacking.h"
#include <cmath>
#include <cstddef>
#include <span>

namespace outshine::Render {
namespace {
Vec4f LightVector(const auto &source, float tail = 0) {
  return {{static_cast<float>(source[0]),
           static_cast<float>(source[1]),
           static_cast<float>(source[2]),
           tail}};
}
}

LightingUniform PackLightingUniform(std::span<const SceneLight> placed,
                                    const EnvironmentLighting &environment,
                                    const FrameContext &frame) {
  LightingUniform packed{};
  packed.Count[0] = static_cast<float>(placed.size());
  packed.Environment = LightVector(environment.RadianceLinear);
  packed.Bounced = LightVector(environment.GroundLinear);
  packed.Up = LightVector(environment.UpUnit);
  packed.SkyGround = LightVector(environment.GroundAlbedo);
  packed.SkyGround[3] = static_cast<float>(environment.CosSunZenith);
  packed.ViewPosition = frame.ViewPosition;
  for (size_t at = 0; at < placed.size(); ++at) {
    const PunctualLight &light = placed[at].Light;
    auto &entry = packed.Items[at];
    entry.Tint = LightVector(light.Colour, static_cast<float>(light.Kind));
    for (int channel = 0; channel < 3; ++channel) { entry.Tint[channel] *= light.Intensity; }
    for (int axis = 0; axis < 3; ++axis) {
      entry.Place[axis] =
          static_cast<float>(placed[at].PositionM[axis] + frame.PreViewTranslation[axis]);
    }
    entry.Place[3] = light.RangeM > 0.0f ? 1.0f / light.RangeM : 0.0f;
    entry.Beam = LightVector(light.Direction);
    const float outer = std::cos(light.OuterConeRad);
    const float inner = std::cos(light.InnerConeRad);
    entry.Cone = {{outer, inner > outer ? 1.0f / (inner - outer) : 0.0f, 0, 0}};
  }
  return packed;
}
}
