#ifndef OUTSHINE_GENERATORS_ROAD_MESHING_ROADMESHER_H
#define OUTSHINE_GENERATORS_ROAD_MESHING_ROADMESHER_H

#include <cstdint>
#include <array>
#include <optional>
#include <span>
#include <vector>

#include "TransportGeometry.h"
#include "Vec3.h"
#include "Ribbon.h"

namespace outshine {

struct RoadMeshBuffers {
  std::vector<float> PositionM;
  std::vector<float> NormalM;
  std::vector<float> ColourRgba;
  std::vector<uint32_t> Index;
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
  RibbonForm Form = RibbonForm::ClosedShell;
  std::array<std::optional<RoadPort>, 2> EndPorts{};
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
