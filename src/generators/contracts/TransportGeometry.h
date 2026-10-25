#ifndef OUTSHINE_GENERATORS_CONTRACTS_TRANSPORTGEOMETRY_H
#define OUTSHINE_GENERATORS_CONTRACTS_TRANSPORTGEOMETRY_H

#include <cstdint>

namespace outshine {
inline constexpr double kCrossfall = 0.025;
inline constexpr double kPavementLipM = 0.05;
inline constexpr double kSealedDepthM = 0.30;

enum class RoadProfile : uint8_t { Rounded, Simple, Kerbed };

struct RoadStation {
  double EastM = 0.0;
  double NorthM = 0.0;
  double GradeM = 0.0;
  uint64_t Node = 0;
};

struct RoadGate {
  double EastM = 0.0;
  double NorthM = 0.0;
  double GradeM = 0.0;
  double OutE = 0.0;
  double OutN = 0.0;
  double HalfWidthM = 0.0;
  double ShoulderM = 0.0;
  double ThicknessM = kSealedDepthM;
};

struct RoadPlane {
  double SlopeE = 0.0;
  double SlopeN = 0.0;
};

struct RoadPort {
  RoadGate Gate;
  RoadPlane Plane;
};

}
#endif
