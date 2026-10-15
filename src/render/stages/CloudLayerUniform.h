#ifndef OUTSHINE_RENDER_STAGES_CLOUDLAYERUNIFORM_H
#define OUTSHINE_RENDER_STAGES_CLOUDLAYERUNIFORM_H

#include "CloudLayer.h"
#include "math/Vec4.h"

#include <cstddef>

namespace outshine::Render {

struct alignas(16) CloudLayerUniform {
  Vec4f Shell;
  Vec4f Field;
  Vec4f Drift;

  [[nodiscard]] constexpr bool operator==(const CloudLayerUniform &) const = default;
};

[[nodiscard]] inline CloudLayerUniform PackCloudLayer(const CloudLayer &layer,
                                                      double seconds) noexcept {
  return {.Shell = {{static_cast<float>(layer.RadiusM),
                     static_cast<float>(layer.BaseM),
                     static_cast<float>(layer.ThicknessM),
                     static_cast<float>(layer.ExtinctionPerM)}},
          .Field = {{static_cast<float>(layer.Cover), 0.0f, 0.0f, 0.0f}},
          .Drift = {{static_cast<float>(layer.WindMs[0] * seconds),
                     static_cast<float>(layer.WindMs[1] * seconds),
                     static_cast<float>(layer.WindMs[2] * seconds),
                     0.0f}}};
}

static_assert(sizeof(CloudLayerUniform) == 3 * sizeof(Vec4f) && alignof(CloudLayerUniform) == 16);
static_assert(offsetof(CloudLayerUniform, Shell) == 0);
static_assert(offsetof(CloudLayerUniform, Field) == 16);
static_assert(offsetof(CloudLayerUniform, Drift) == 32);

}
#endif
