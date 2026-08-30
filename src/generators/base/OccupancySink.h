#ifndef OUTSHINE_GENERATORS_BASE_OCCUPANCYSINK_H
#define OUTSHINE_GENERATORS_BASE_OCCUPANCYSINK_H

#include <cstdint>

#include "ContactMaterial.h"
#include "Claim.h"
#include "Ground.h"
#include "Span.h"

namespace outshine::Generators {

class OccupancySink {
public:
  struct Storage {
    Span<Body> Bodies;
    Span<uint32_t> Links;
    Span<uint32_t> Cells;
    double CellM = 0.0;
  };

  explicit OccupancySink(const Storage &storage);

  [[nodiscard]] uint32_t Capacity() const noexcept { return (uint32_t)Store_.Bodies.Size(); }

  [[nodiscard]] Claim Place(const Body &body) noexcept;

  [[nodiscard]] Span<const Body> Placed() const noexcept { return Store_.Bodies.Sub(0, Count()); }

  [[nodiscard]] uint32_t Claims(Claim::Outcome why) const noexcept { return Claims_[(size_t)why]; }

  static int Cells(double spanM, double cellM);

private:
  friend class RegionPool;
  void Open(const Ground &ground) noexcept;

  uint32_t &Count() noexcept { return Claims_[(size_t)Claim::Outcome::Placed]; }

  [[nodiscard]] uint32_t Count() const noexcept { return Claims_[(size_t)Claim::Outcome::Placed]; }

  [[nodiscard]] int CellOf(double m, int cells) const noexcept;
  [[nodiscard]] bool Clear(const Body &body) const noexcept;

  Storage Store_;
  double SpanEm_ = 0.0, SpanNm_ = 0.0;
  int CellsE_ = 0, CellsN_ = 0;
  uint32_t Claims_[Claim::kOutcomes] = {0, 0, 0, 0};
  float MaxRadiusM_ = 0.0f;
};

}
#endif
