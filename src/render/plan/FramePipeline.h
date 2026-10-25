#ifndef OUTSHINE_RENDER_PLAN_FRAMEPIPELINE_H
#define OUTSHINE_RENDER_PLAN_FRAMEPIPELINE_H

#include "ExecutionPlan.h"
#include <expected>
#include <span>
#include <string>
#include <string_view>

namespace outshine::Render {

struct FramePipelineSettings {
  std::span<const std::string> Stages;
  std::span<const std::string> Outputs;
  std::string_view Transfer;
  std::string_view Precision;
  std::string_view Antialiasing;
  bool DrawsSky = false;
  bool Presents = false;
  bool Transmissive = false;
  double Exposure = 0;
  double IlluminanceLx = 0;
};

[[nodiscard]] std::expected<PlanSpec, std::string>
BuildFramePipeline(const FramePipelineSettings &settings);

}
#endif
