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

  struct FaceBounds {
    double NormalizedGradientMaximum = 0.0;
    double HeightChordErrorM = 0.0;
  };

  enum class Error : uint8_t {
    InputBudget,
    InvalidPlane,
    InvalidPatch,
    InvalidQuery,
    NonpositiveDenominator,
    InvalidGradeFrame,
    NumericRange
  };

  [[nodiscard]] static std::expected<RoadPortField, Error> Make(std::span<const Plane> planes,
                                                                std::span<const Patch> patches);

  [[nodiscard]] std::expected<Sample, Error> Evaluate(size_t patch, EastNorth at) const noexcept;

  [[nodiscard]] std::expected<FaceBounds, Error>
  Bounds(size_t patch,
         const std::array<EastNorth, 3> &triangle,
         const std::array<double, 4> &gradeFrame) const noexcept;

private:
  RoadPortField(std::vector<Plane> planes, std::vector<Patch> patches);

  struct Basis {
    std::array<double, 6> Height{};
    std::array<std::array<double, 6>, 2> Gradient{};
    double Denominator = 1.0;
  };

  struct Control {
    std::array<double, 6> Error{};
    std::array<std::array<double, 6>, 2> Gradient{};
  };

  [[nodiscard]] std::expected<Basis, Error> BasisAt(size_t patch, EastNorth at) const noexcept;
  [[nodiscard]] std::expected<std::array<Control, 15>, Error>
  Controls(size_t patch, const std::array<EastNorth, 3> &triangle) const noexcept;

  std::vector<Plane> Planes_;
  std::vector<Patch> Patches_;
};

}
#endif
