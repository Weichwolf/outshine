#ifndef OUTSHINE_GENERATORS_DRAW_ROADMESH_H
#define OUTSHINE_GENERATORS_DRAW_ROADMESH_H

#include <span>
#include <cstdint>
#include <vector>

#include "math/Vec3.h"

namespace outshine::Generators {

inline constexpr double kCrossfall = 0.025;

enum class RoadProfile : uint8_t { Rounded, Simple, Kerbed };

struct RoadStation {
  double EastM = 0.0;
  double SouthM = 0.0;
  double GradeM = 0.0;
  uint64_t Node = 0;
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

void RaiseJunction(std::span<const RoadGate> gates, const Vec3f &wearsLinear, RoadRaised &into);

struct RoadRefusals {
  size_t Fit = 0;
  size_t Rise = 0;
  size_t Bank = 0;
  size_t Sweep = 0;
  size_t TooShort = 0;
};

void DesignProfile(std::span<RoadStation> along, double mostGradient, double leastCrestK);

void SweepRoad(std::span<const RoadStation> along,
               double halfWidthM,
               RoadProfile profile,
               const Vec3f &wearsLinear,
               double crossfall,
               RoadRaised &into,
               size_t *piecesLaid,
               size_t *cutsMade,
               size_t *piecesRefused,
               RoadRefusals *why);

} // namespace outshine::Generators
#endif
