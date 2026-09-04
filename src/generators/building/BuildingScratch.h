#ifndef OUTSHINE_GENERATORS_BUILDING_BUILDINGSCRATCH_H
#define OUTSHINE_GENERATORS_BUILDING_BUILDINGSCRATCH_H

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

#include "BuildingShape.h"
#include "FlatMap.h"
#include "StructureMesher.h"

namespace outshine::Generators {

inline constexpr int kMaxParts = 9;

struct Piece {
  std::vector<En> P;
  std::vector<uint8_t> Party;
};

template <typename T> class Slots {
public:
  [[nodiscard]] T &Next() {
    if (Count_ == Held_.size()) { Held_.emplace_back(); }
    return Held_[Count_++];
  }

  void Reset() noexcept { Count_ = 0; }

  [[nodiscard]] std::span<T> Standing() noexcept { return {Held_.data(), Count_}; }

  [[nodiscard]] size_t Count() const noexcept { return Count_; }

  void Swap(Slots &other) noexcept {
    Held_.swap(other.Held_);
    std::swap(Count_, other.Count_);
  }

private:
  std::vector<T> Held_;
  size_t Count_ = 0;
};

struct BuildingScratch final : MeshScratch {
  FlatMap<uint32_t> Welded;
  std::array<FlatMap<uint32_t>, 2> Corners;

  std::vector<En> Outline;
  Piece Whole, Rest, Plot, Beyond, Lo, Hi, Main, Wing, Cap;
  std::array<Piece, kMaxParts> Row;
  std::vector<double> Side;
  std::vector<int> Sign;
  BuildingShape One, Made;
  Slots<BuildingShape> Parts, Stacked;
  std::vector<En> Inner;

  std::vector<En> Overhang, CrownInner, CrownOut, Proud, Foot, Wide, Covered, Tris, Refined;
  std::vector<double> Breaks, Other, At;

  std::vector<uint32_t> Poly;
  Slots<std::vector<En>> Cells, NextCells;
  std::vector<En> Mine, Above, Below;

  void ClearWelds() noexcept {
    Welded.Clear();
    Corners[0].Clear();
    Corners[1].Clear();
  }
};

} // namespace outshine::Generators
#endif
