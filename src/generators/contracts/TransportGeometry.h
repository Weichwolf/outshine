#ifndef OUTSHINE_GENERATORS_CONTRACTS_TRANSPORTGEOMETRY_H
#define OUTSHINE_GENERATORS_CONTRACTS_TRANSPORTGEOMETRY_H

#include <cstddef>
#include <cstdint>
#include <functional>
#include <span>

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

enum class RoadDeckSupport : uint8_t { Terrain, Interior, Roof };

using RoadDeckProfile = std::function<RoadDeckSupport(size_t, std::span<RoadStation>, double)>;

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
