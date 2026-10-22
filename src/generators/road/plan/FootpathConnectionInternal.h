#ifndef OUTSHINE_GENERATORS_ROAD_PLAN_FOOTPATHCONNECTIONINTERNAL_H
#define OUTSHINE_GENERATORS_ROAD_PLAN_FOOTPATHCONNECTIONINTERNAL_H

#include "FootpathConnectionPlan.h"
#include "PlanHierarchy.h"

namespace outshine::Generators::FootpathConnections {

struct Inputs {
  std::vector<double> Lengths;
  size_t Segments = 0;
  size_t Endpoints = 0;
  size_t EligibleEndpoints = 0;
};

struct Segment {
  uint32_t Source = 0;
  uint32_t Index = 0;
  double StationM = 0.0;
  double LengthM = 0.0;
};

struct Index {
  PlanHierarchy Hierarchy;
  std::vector<Segment> Segments;
  size_t BuildHeapBytes = 0;
};

[[nodiscard]] bool GroundFoot(const FootpathSource &source);
[[nodiscard]] FootpathEndDisposition Disposition(const FootpathSource &source, size_t end);
[[nodiscard]] std::expected<Inputs, FootpathConnectionError>
PrepareInputs(std::span<const FootpathSource> sources,
              FootpathConnectionRecipe recipe,
              FootpathConnectionBudget budget,
              const std::stop_token &stop);
[[nodiscard]] std::expected<Index, FootpathConnectionError>
BuildIndex(std::span<const FootpathSource> sources, const std::stop_token &stop);
[[nodiscard]] std::optional<FootpathConnection> Candidate(std::span<const FootpathSource> sources,
                                                          std::span<const double> lengths,
                                                          uint32_t branch,
                                                          uint32_t end,
                                                          const Segment &segment,
                                                          FootpathConnectionRecipe recipe);

[[nodiscard]] std::optional<FootpathConnection> Choose(std::span<const FootpathSource> sources,
                                                       const Inputs &inputs,
                                                       const Index &index,
                                                       uint32_t branch,
                                                       uint32_t end,
                                                       std::span<const uint32_t> candidates,
                                                       FootpathConnectionRecipe recipe,
                                                       FootpathEndDisposition &disposition);

}

#endif
