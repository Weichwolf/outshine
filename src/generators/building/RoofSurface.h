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
  RoofSurface(const RoofSurface &) = delete;
  RoofSurface &operator=(const RoofSurface &) = delete;
  RoofSurface(RoofSurface &&) = delete;
  RoofSurface &operator=(RoofSurface &&) = delete;
  [[nodiscard]] static std::expected<void, HipRoofError> PrepareHip(BuildingShape &shape);

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

  [[nodiscard]] bool Cover(std::span<const EastNorth> plan,
                           BuildingScratch &scratch,
                           std::vector<EastNorth> &tris) const;

  void BreaksAlong(const EastNorth &from, const EastNorth &to, std::vector<double> &at) const;

  static bool Fill(std::span<const EastNorth> requested,
                   BuildingScratch &scratch,
                   std::vector<EastNorth> &triangles,
                   std::span<const std::vector<EastNorth>> holes = {});

  static void Widened(std::span<const EastNorth> ring,
                      double byM,
                      std::span<const uint8_t> held,
                      std::vector<EastNorth> &out);

private:
  const BuildingShape &Shape_;
  std::optional<std::expected<HipRoofPlan, HipRoofError>> LocalHip_;
  const std::expected<HipRoofPlan, HipRoofError> *Hip_ = nullptr;

  [[nodiscard]] double HipHeightAt(EastNorth point) const noexcept;
  [[nodiscard]] std::optional<Hit> TraceHip(const Vec3 &origin,
                                            const Vec3 &direction,
                                            double minimum,
                                            double maximum) const noexcept;
  [[nodiscard]] bool CoverHip(std::span<const EastNorth> requested,
                              BuildingScratch &scratch,
                              std::vector<EastNorth> &triangles) const;
  void HipBreaksAlong(EastNorth from, EastNorth to, std::vector<double> &at) const;
};

}
#endif
