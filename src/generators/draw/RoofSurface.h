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
  [[nodiscard]] static size_t UnclippedTaken() { return Unclipped_.exchange(0u); }

  [[nodiscard]] static size_t OutsideTaken() { return Outside_.exchange(0u); }

  [[nodiscard]] static size_t BreaksKeptTaken();
  [[nodiscard]] static size_t BreaksDroppedTaken();
  [[nodiscard]] static size_t BreaksMergedTaken();

  explicit RoofSurface(const BuildingShape &shape);

  [[nodiscard]] double HeightAt(const En &enu) const noexcept;

  void Cover(std::span<const En> plan, std::vector<En> &tris) const;

  void BreaksAlong(const En &from, const En &to, std::vector<double> &at) const;

  static bool Fill(std::span<const En> plan, std::vector<En> &tris);

  static std::vector<En>
  Widened(std::span<const En> ring, double byM, std::span<const uint8_t> held = {});

private:
  inline static std::atomic<size_t> Unclipped_{0};
  inline static std::atomic<size_t> Outside_{0};
  const BuildingShape &Shape_;
};

} // namespace outshine::Generators
#endif
