#ifndef OUTSHINE_GENERATORS_BUILDING_ROOFSURFACE_H
#define OUTSHINE_GENERATORS_BUILDING_ROOFSURFACE_H

#include <vector>
#include <cstdint>
#include <span>
#include <optional>
#include "math/Vec3.h"

#include "BuildingScratch.h"

namespace outshine::Generators {

class RoofSurface {
public:
  explicit RoofSurface(const BuildingShape &shape);

  [[nodiscard]] double HeightAt(const EastNorth &enu) const noexcept;

  struct Hit {
    double Along = 0.0;
    Vec3 Normal;
  };

  [[nodiscard]] bool Contains(const EastNorth &enu) const noexcept;
  [[nodiscard]] std::optional<Hit> Trace(const Vec3 &origin,
                                         const Vec3 &direction,
                                         double minimum,
                                         double maximum,
                                         std::vector<double> &cuts) const;

  void Cover(std::span<const EastNorth> plan,
             BuildingScratch &scratch,
             std::vector<EastNorth> &tris) const;

  void BreaksAlong(const EastNorth &from, const EastNorth &to, std::vector<double> &at) const;

  static bool Fill(std::span<const EastNorth> plan,
                   BuildingScratch &scratch,
                   std::vector<EastNorth> &tris,
                   std::span<const std::vector<EastNorth>> holes = {});

  static void Widened(std::span<const EastNorth> ring,
                      double byM,
                      std::span<const uint8_t> held,
                      std::vector<EastNorth> &out);

private:
  const BuildingShape &Shape_;
};

}
#endif
