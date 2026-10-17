#ifndef OUTSHINE_GENERATORS_FLORA_PLANTSHOOT_H
#define OUTSHINE_GENERATORS_FLORA_PLANTSHOOT_H

#include <array>
#include <optional>

namespace outshine::Generators {

struct PlantLeafPose {
  std::array<double, 3> OriginM{}, NodeM{};
  std::array<double, 9> Frame{};
  double StemParameter = 0;
};

struct PlantShoot {
  struct Placement {
    double Parameter = 0, AzimuthRad = 0;
  };

  double LengthM = 0, BendRad = 0, PetioleM = 0, LeafAngleRad = 0, NormalAlignment = 0;
  std::array<double, 3> NormalHint{0, 0, 1};
  [[nodiscard]] std::optional<PlantLeafPose> LeafAt(Placement placement) const noexcept;
};

}
#endif
