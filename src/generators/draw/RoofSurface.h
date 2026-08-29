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
  // ROOF TRIANGLES WITH A VERTEX OUTSIDE THEIR OWN FOOTPRINT, since the last read.
  [[nodiscard]] static size_t OutsideTaken() { return Outside_.exchange(0u); }

  explicit RoofSurface(const BuildingShape &shape);

  [[nodiscard]] double HeightAt(const En &enu) const noexcept;

  void Cover(std::span<const En> plan, std::vector<En> &tris) const;

  // WHERE THE ROOF BREAKS ACROSS ONE EDGE OF THE FOOTPRINT, as parameters in (0, 1) along it,
  // sorted. `Cover` splits the covering along exactly these lines, so anything that has to meet the
  // covering's boundary -- a gable, a soffit -- has to break at the same places or its edges cannot
  // pair with it. A ridge lies on the MIDLINE of a gable end, never at its corners, which is why
  // reading the two corner heights saw nothing to build there.
  void BreaksAlong(const En &from, const En &to, std::vector<double> &at) const;

  static std::vector<En> Widened(std::span<const En> ring, double byM);

private:
  inline static std::atomic<size_t> Unclipped_{0};
  inline static std::atomic<size_t> Outside_{0};
  const BuildingShape &Shape_;
};

}
#endif
