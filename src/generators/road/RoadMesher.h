#ifndef OUTSHINE_GENERATORS_ROAD_ROADMESHER_H
#define OUTSHINE_GENERATORS_ROAD_ROADMESHER_H

#include <cstdint>
#include <span>
#include <vector>

#include "math/Vec3.h"

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

struct RoadMeshBuffers {
  std::vector<float> PositionM;
  std::vector<float> NormalM;
  std::vector<float> ColourRgba;
  std::vector<uint32_t> Index;
};

struct RoadGate {
  double EastM = 0.0;
  double NorthM = 0.0;
  double GradeM = 0.0;
  double OutE = 0.0;
  double OutN = 0.0;
  double HalfWidthM = 0.0;
};

struct RoadPlane {
  double SlopeE = 0.0;
  double SlopeN = 0.0;
};

struct RoadMeshingRejections {
  size_t Fit = 0;
  size_t Rise = 0;
  size_t Bank = 0;
  size_t Sweep = 0;
  size_t TooShort = 0;
};

struct RoadMeshingStats {
  size_t Pieces = 0;
  size_t Cuts = 0;
  size_t Refused = 0;
  RoadMeshingRejections Rejections;

  RoadMeshingStats &operator+=(const RoadMeshingStats &more) {
    Pieces += more.Pieces;
    Cuts += more.Cuts;
    Refused += more.Refused;
    Rejections.Fit += more.Rejections.Fit;
    Rejections.Rise += more.Rejections.Rise;
    Rejections.Bank += more.Rejections.Bank;
    Rejections.Sweep += more.Rejections.Sweep;
    Rejections.TooShort += more.Rejections.TooShort;
    return *this;
  }
};

struct RoadSweep {
  double HalfWidthM = 0.0;
  RoadProfile Profile = RoadProfile::Rounded;
  Vec3f WearsLinear;
  double Crossfall = 0.0;
};

class RoadMesher {
public:
  virtual ~RoadMesher() = default;
  RoadMesher(const RoadMesher &) = delete;
  RoadMesher &operator=(const RoadMesher &) = delete;

  [[nodiscard]] virtual RoadMeshingStats
  Sweep(std::span<const RoadStation> along, RoadSweep how, RoadMeshBuffers &into) const = 0;

  virtual void Junction(std::span<const RoadGate> gates,
                        RoadPlane plane,
                        const Vec3f &wearsLinear,
                        RoadMeshBuffers &into) const = 0;

protected:
  RoadMesher() = default;
};

}
#endif
