#ifndef OUTSHINE_GENERATORS_DRAW_ROADMESH_H
#define OUTSHINE_GENERATORS_DRAW_ROADMESH_H

#include <cstdint>
#include <vector>

#include "Span.h"

namespace outshine::Generators {

enum class RoadProfile : uint8_t { Rounded, Simple, Kerbed };

struct RoadStation {
  double EastM = 0.0;
  double SouthM = 0.0;
  double GradeM = 0.0;
};

struct RoadRaised {
  std::vector<float> PositionM;
  std::vector<float> NormalM;
  std::vector<float> ColourRgba;
  std::vector<uint32_t> Index;
};

struct RoadGate {
  double EastM = 0.0;
  double SouthM = 0.0;
  double GradeM = 0.0;
  double OutE = 0.0;
  double OutS = 0.0;
  double HalfWidthM = 0.0;
};

void RaiseJunction(Span<const RoadGate> gates, const float wearsLinear[3], RoadRaised &into);

void RaiseRoad(Span<const RoadStation> along,
               double halfWidthM,
               RoadProfile profile,
               const float wearsLinear[3],
               RoadRaised &into);

} // namespace outshine::Generators
#endif
