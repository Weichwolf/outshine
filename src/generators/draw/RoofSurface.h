#ifndef OUTSHINE_GENERATORS_DRAW_ROOFSURFACE_H
#define OUTSHINE_GENERATORS_DRAW_ROOFSURFACE_H

#include <vector>
#include <atomic>
#include <span>

#include "BuildingShape.h"

namespace outshine::Generators {

class RoofSurface {
public:
  // ROOFS THAT COULD NOT BE COVERED, since the last read. A silent partial triangulation is what
  // this exists to make visible; the reader takes it and clears it.
  [[nodiscard]] static size_t UnclippedTaken() { return Unclipped_.exchange(0u); }

  explicit RoofSurface(const BuildingShape &shape);

  [[nodiscard]] double HeightAt(const En &enu) const noexcept;

  void Cover(std::span<const En> plan, std::vector<En> &tris) const;

  static std::vector<En> Widened(std::span<const En> ring, double byM);

private:
  inline static std::atomic<size_t> Unclipped_{0};
  const BuildingShape &Shape_;
};

}
#endif
