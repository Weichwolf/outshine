#ifndef OUTSHINE_GENERATORS_DRAW_ROOFSURFACE_H
#define OUTSHINE_GENERATORS_DRAW_ROOFSURFACE_H

#include <vector>
#include <atomic>
#include <cstdint>
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
  // WHAT `BreaksAlong` DID WITH THE CREASE CROSSINGS IT FOUND, since the last read. Two thresholds
  // act on one edge and they move in OPPOSITE directions with its length -- a crossing is dropped
  // within `kWeldM` METRES of an end, which is a shrinking FRACTION as the edge grows, and two
  // crossings are merged within 1e-3 of the PARAMETER, which is a growing distance. The break
  // between a clean 8 m footprint and a holed 9 m one has to show in one of these three.
  [[nodiscard]] static size_t BreaksKeptTaken();
  [[nodiscard]] static size_t BreaksDroppedTaken();
  [[nodiscard]] static size_t BreaksMergedTaken();

  explicit RoofSurface(const BuildingShape &shape);

  [[nodiscard]] double HeightAt(const En &enu) const noexcept;

  void Cover(std::span<const En> plan, std::vector<En> &tris) const;

  // WHERE THE ROOF BREAKS ACROSS ONE EDGE OF THE FOOTPRINT, as parameters in (0, 1) along it,
  // sorted. `Cover` splits the covering along exactly these lines, so anything that has to meet the
  // covering's boundary -- a gable, a soffit -- has to break at the same places or its edges cannot
  // pair with it. A ridge lies on the MIDLINE of a gable end, never at its corners, which is why
  // reading the two corner heights saw nothing to build there.
  void BreaksAlong(const En &from, const En &to, std::vector<double> &at) const;

  // A FLAT LID IS NOT A ROOF. `Cover` splits what it triangulates along the roof's crease lines,
  // which is right for a covering and wrong for a floor: the creases are taken in BOX coordinates,
  // so on a ring offset from the footprint they cross at a different place and the lid's boundary
  // stops matching the wall standing on it.
  static bool Fill(std::span<const En> plan, std::vector<En> &tris);

  static std::vector<En> Widened(std::span<const En> ring, double byM,
                                 std::span<const uint8_t> held = {});

private:
  inline static std::atomic<size_t> Unclipped_{0};
  inline static std::atomic<size_t> Outside_{0};
  const BuildingShape &Shape_;
};

}
#endif
