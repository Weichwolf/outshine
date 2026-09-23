#ifndef OUTSHINE_GENERATORS_BUILDING_ROOFSURFACE_H
#define OUTSHINE_GENERATORS_BUILDING_ROOFSURFACE_H

#include <vector>
#include <cstdint>
#include <span>

#include "BuildingScratch.h"

namespace outshine::Generators {

class RoofSurface {
public:
  explicit RoofSurface(const BuildingShape &shape);

  [[nodiscard]] double HeightAt(const EastNorth &enu) const noexcept;

  void Cover(std::span<const EastNorth> plan,
             BuildingScratch &scratch,
             std::vector<EastNorth> &tris) const;

  void BreaksAlong(const EastNorth &from, const EastNorth &to, std::vector<double> &at) const;

  static bool
  Fill(std::span<const EastNorth> plan, BuildingScratch &scratch, std::vector<EastNorth> &tris);

  static void Widened(std::span<const EastNorth> ring,
                      double byM,
                      std::span<const uint8_t> held,
                      std::vector<EastNorth> &out);

private:
  const BuildingShape &Shape_;
};

}
#endif
