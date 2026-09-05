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

struct RoadRaised {
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

struct RoadRefusals {
  size_t Fit = 0;
  size_t Rise = 0;
  size_t Bank = 0;
  size_t Sweep = 0;
  size_t TooShort = 0;
};

struct RoadTallied {
  size_t Pieces = 0;
  size_t Cuts = 0;
  size_t Refused = 0;
  RoadRefusals Why;

  RoadTallied &operator+=(const RoadTallied &more) {
    Pieces += more.Pieces;
    Cuts += more.Cuts;
    Refused += more.Refused;
    Why.Fit += more.Why.Fit;
    Why.Rise += more.Why.Rise;
    Why.Bank += more.Why.Bank;
    Why.Sweep += more.Why.Sweep;
    Why.TooShort += more.Why.TooShort;
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

  [[nodiscard]] virtual RoadTallied
  Sweep(std::span<const RoadStation> along, RoadSweep how, RoadRaised &into) const = 0;

  virtual void Junction(std::span<const RoadGate> gates,
                        RoadPlane plane,
                        const Vec3f &wearsLinear,
                        RoadRaised &into) const = 0;

protected:
  RoadMesher() = default;
};

} // namespace outshine
#endif
