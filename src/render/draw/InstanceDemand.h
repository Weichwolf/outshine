#ifndef OUTSHINE_RENDER_DRAW_INSTANCEDEMAND_H
#define OUTSHINE_RENDER_DRAW_INSTANCEDEMAND_H

#include "BatchVisibility.h"
#include "Vec3.h"
#include "ProjectedErrorBudget.h"
#include <cstdint>
#include <expected>
#include <span>
#include <vector>

namespace outshine::Render {
enum class InstanceDetail : uint8_t { Resolved, Unresolved, Unknown };

struct InstancePatchNeed {
  uint32_t Index = 0;
  InstanceDetail Detail = InstanceDetail::Unknown;
  bool Visible = false;
  bool Caster = false;
};

struct InstanceDemand {
  Vec3 EyeM;
  ProjectedErrorBudget Projection;
  std::span<const BatchFrustum> Views;
  std::span<const BatchFrustum> Casters;
};

enum class InstanceDemandError { InvalidCount, InvalidView };

[[nodiscard]] std::expected<std::vector<InstancePatchNeed>, InstanceDemandError>
PlanInstancePatches(std::span<const BatchSphere> bounds,
                    std::span<const double> errorsM,
                    const InstanceDemand &demand);
}
#endif
