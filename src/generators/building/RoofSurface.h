#ifndef OUTSHINE_GENERATORS_BUILDING_ROOFSURFACE_H
#define OUTSHINE_GENERATORS_BUILDING_ROOFSURFACE_H

#include <vector>
#include <cstdint>
#include <span>

#include "BuildingShape.h"

namespace outshine::Generators {

class RoofSurface {
public:
  explicit RoofSurface(const BuildingShape &shape);

  [[nodiscard]] double HeightAt(const En &enu) const noexcept;

  void Cover(std::span<const En> plan, std::vector<En> &tris) const;

  void BreaksAlong(const En &from, const En &to, std::vector<double> &at) const;

  static bool Fill(std::span<const En> plan, std::vector<En> &tris);

  static std::vector<En>
  Widened(std::span<const En> ring, double byM, std::span<const uint8_t> held = {});

private:
  const BuildingShape &Shape_;
};

} // namespace outshine::Generators
#endif
