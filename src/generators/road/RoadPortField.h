#ifndef OUTSHINE_GENERATORS_ROAD_ROADPORTFIELD_H
#define OUTSHINE_GENERATORS_ROAD_ROADPORTFIELD_H

#include "Earth.h"
#include "RoadMesher.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <span>
#include <vector>

namespace outshine::Generators {

class RoadPortField {
public:
  struct Plane {
    EastNorth Origin;
    double HeightM = 0.0;
    RoadPlane Gradient;
  };

  struct Patch {
    std::array<uint32_t, 2> Parents{};
    std::array<EastNorth, 2> CutCentres;
    std::array<EastNorth, 2> InwardNormals;
    double ScaleM = 1.0;
  };

  struct Sample {
    double HeightM = 0.0;
    RoadPlane Gradient;
  };

  enum class Error : uint8_t {
    InputBudget,
    InvalidPlane,
    InvalidPatch,
    InvalidQuery,
    NonpositiveDenominator
  };

  [[nodiscard]] static std::expected<RoadPortField, Error> Make(std::span<const Plane> planes,
                                                                std::span<const Patch> patches);

  [[nodiscard]] std::expected<Sample, Error> Evaluate(size_t patch, EastNorth at) const noexcept;

private:
  RoadPortField(std::vector<Plane> planes, std::vector<Patch> patches);

  std::vector<Plane> Planes_;
  std::vector<Patch> Patches_;
};

}
#endif
