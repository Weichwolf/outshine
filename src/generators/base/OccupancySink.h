#ifndef OUTSHINE_GENERATORS_BASE_OCCUPANCYSINK_H
#define OUTSHINE_GENERATORS_BASE_OCCUPANCYSINK_H

#include <span>
#include <array>
#include <cstdint>

#include "ContactMaterial.h"
#include "Claim.h"
#include "Ground.h"

namespace outshine::Generators {

class OccupancySink {
public:
  struct Storage {
    std::span<Solid> Bodies;
    std::span<uint32_t> Links;
    std::span<uint32_t> Cells;
    double CellM = 0.0;
  };

  explicit OccupancySink(const Storage &storage);

  [[nodiscard]] uint32_t Capacity() const noexcept {
    return static_cast<uint32_t>(Store_.Bodies.size());
  }

  [[nodiscard]] Claim Place(const Solid &body) noexcept;

  [[nodiscard]] std::span<const Solid> Placed() const noexcept {
    return Store_.Bodies.subspan(0, Count());
  }

  [[nodiscard]] uint32_t Claims(Claim::Outcome why) const noexcept {
    return Claims_[static_cast<size_t>(why)];
  }

  static int Cells(double spanM, double cellM);

private:
  friend class RegionPool;
  void Open(const Ground &ground) noexcept;

  uint32_t &Count() noexcept { return Claims_[static_cast<size_t>(Claim::Outcome::Placed)]; }

  [[nodiscard]] uint32_t Count() const noexcept {
    return Claims_[static_cast<size_t>(Claim::Outcome::Placed)];
  }

  enum class Axis : uint8_t { East, North };

  [[nodiscard]] int CellOf(double m, Axis on) const noexcept;
  [[nodiscard]] bool Clear(const Solid &body) const noexcept;

  Storage Store_;
  double SpanEm_ = 0.0, SpanNm_ = 0.0;
  int CellsE_ = 0, CellsN_ = 0;
  std::array<uint32_t, Claim::kOutcomes> Claims_ = {{0, 0, 0, 0}};
  float MaxRadiusM_ = 0.0f;
};

} // namespace outshine::Generators
#endif
