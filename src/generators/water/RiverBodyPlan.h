#ifndef OUTSHINE_GENERATORS_WATER_RIVERBODYPLAN_H
#define OUTSHINE_GENERATORS_WATER_RIVERBODYPLAN_H

#include "WaterSurfaceTopology.h"
#include "PlanHierarchy.h"
#include <cstdint>
#include <expected>
#include <stop_token>
#include <vector>
#include <array>
#include <optional>

namespace outshine::Generators {
enum class WaterGroundPolicy { SeparateInputs, AssumeUnlabelledGround };

struct RiverBody {
  WaterSurfaceTopology Surface;
  PlanHierarchy FaceIndex;
  std::vector<uint32_t> Courses;
};

struct RiverBodyPlan {
  std::vector<RiverBody> Bodies;
  std::vector<WaterAsset::SurfaceRejection> Rejections;
  uint64_t EnvelopePairs = 0;
  uint64_t CourseTriangleTests = 0;
  WaterRoundoffReport Roundoff;
};

[[nodiscard]] std::expected<RiverBodyPlan, WaterTopologyError>
PlanRiverBodies(const WaterAsset &water,
                const TangentFrame &frame,
                WaterGroundPolicy policy,
                WaterTopologyBudget budget = {},
                const std::stop_token &stop = {});

[[nodiscard]] std::expected<void, WaterTopologyError> BindRiverCourses(const WaterAsset &water,
                                                                       const TangentFrame &frame,
                                                                       RiverBodyPlan &plan,
                                                                       const std::stop_token &stop);
[[nodiscard]] std::optional<double>
WaterCourseFaceEntry(const WaterSurfaceTopology &topology,
                     uint32_t face,
                     const std::array<std::array<double, 2>, 2> &segment);
}
#endif
