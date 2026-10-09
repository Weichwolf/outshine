#ifndef OUTSHINE_GENERATORS_ROAD_ROADHEIGHTPLAN_H
#define OUTSHINE_GENERATORS_ROAD_ROADHEIGHTPLAN_H

#include <cstdint>
#include <expected>
#include <limits>
#include <span>
#include <string_view>
#include <vector>

namespace outshine::Generators {

struct RoadHeightNode {
  double LowSampleM = 0.0;
  double HighSampleM = 0.0;
  double MinimumM = -std::numeric_limits<double>::infinity();
  double MaximumM = std::numeric_limits<double>::infinity();
};

struct RoadHeightLink {
  uint32_t First = 0;
  uint32_t Second = 0;
  double MaximumRiseM = 0.0;
  double FirstOffsetM = 0.0;
  double SecondOffsetM = 0.0;
};

struct RoadHeightPlan {
  std::vector<double> HeightM;
  double MaximumAdjustmentM = 0.0;
};

[[nodiscard]] std::expected<RoadHeightPlan, std::string_view>
PlanRoadHeights(std::span<const RoadHeightNode> nodes, std::span<const RoadHeightLink> links);

}

#endif
