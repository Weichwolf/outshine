#ifndef OCCUPANCYSINK_H
#define OCCUPANCYSINK_H

#include <cstdint>

#include "Body.h"
#include "Claim.h"
#include "Ground.h"
#include "Span.h"

namespace outshine::Generators {

class OccupancySink {
public:
  /* The buffers alone — how far they reach is the region's statement, taken at Open(). */
  struct Storage {
    Span<Body> Bodies;
    Span<uint32_t> Links;   /* one per body slot */
    Span<uint32_t> Cells;   /* at least Cells(SpanEm) * Cells(SpanNm) heads into Links */
    double CellM = 0.0;
  };

  explicit OccupancySink(const Storage &storage);

  uint32_t Capacity() const noexcept { return (uint32_t)Store_.Bodies.Size(); }

  /* THE CLAIM. Placement and conflict are one call: a body whose contact cylinder cuts one already
   * claimed is refused here, so a generator never learns another one exists. */
  [[nodiscard]] Claim Place(const Body &body) noexcept;

  Span<const Body> Placed() const noexcept { return Store_.Bodies.Sub(0, Count()); }
  uint32_t Claims(Claim::Outcome why) const noexcept { return Claims_[(size_t)why]; }

  static int Cells(double spanM, double cellM);

private:
  /* Only a lease hands out an open sink: a buffer set that has just held another region and a
   * region are two halves of one object, and the pool is what puts them together. */
  friend class RegionPool;
  void Open(const Ground &ground) noexcept;

  uint32_t &Count() noexcept { return Claims_[(size_t)Claim::Outcome::Placed]; }
  uint32_t Count() const noexcept { return Claims_[(size_t)Claim::Outcome::Placed]; }
  int CellOf(double m, int cells) const noexcept;
  [[nodiscard]] bool Clear(const Body &body) const noexcept;

  Storage Store_;
  double SpanEm_ = 0.0, SpanNm_ = 0.0;
  int CellsE_ = 0, CellsN_ = 0;
  uint32_t Claims_[Claim::kOutcomes] = {0, 0, 0, 0};
  float MaxRadiusM_ = 0.0f;
};

} // namespace outshine::Generators
#endif
