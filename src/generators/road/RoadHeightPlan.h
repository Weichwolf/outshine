#ifndef OUTSHINE_GENERATORS_ROAD_ROADHEIGHTPLAN_H
#define OUTSHINE_GENERATORS_ROAD_ROADHEIGHTPLAN_H

#include <cstddef>
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

struct RoadHeightClearance {
  uint32_t Lower = 0;
  uint32_t Upper = 0;
  double MinimumGapM = 0.0;
  double LowerOffsetM = 0.0;
  double UpperOffsetM = 0.0;
};

struct RoadHeightPlan {
  std::vector<double> HeightM;
  double MaximumAdjustmentM = 0.0;
};

enum class RoadHeightConstraintKind : uint8_t { ForwardLink, ReverseLink, Clearance };

struct RoadHeightConstraint {
  size_t Index = 0;
  RoadHeightConstraintKind Kind = RoadHeightConstraintKind::ForwardLink;
};

struct RoadHeightFailure {
  std::string_view Reason;
  std::vector<uint32_t> CycleNodes;
  double MaximumOffsetScale = 1.0;
  std::vector<RoadHeightConstraint> CycleConstraints;
};

enum class RoadHeightFit : uint8_t { MinimaxAdjustment, PreferCuts };

[[nodiscard]] std::expected<RoadHeightPlan, RoadHeightFailure>
PlanRoadHeights(std::span<const RoadHeightNode> nodes,
                std::span<const RoadHeightLink> links,
                RoadHeightFit fit = RoadHeightFit::MinimaxAdjustment);

[[nodiscard]] std::expected<RoadHeightPlan, RoadHeightFailure>
PlanRoadHeights(std::span<const RoadHeightNode> nodes,
                std::span<const RoadHeightLink> links,
                std::span<const RoadHeightClearance> clearances,
                RoadHeightFit fit = RoadHeightFit::MinimaxAdjustment);

}

#endif
