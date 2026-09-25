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

struct FootprintPiece {
  std::vector<EastNorth> Ring;
  std::vector<uint8_t> PartyWallEdges;
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

inline constexpr std::array<uint64_t, 5> kBuildingHashMultipliers{
    73856093ULL, 19349663ULL, 83492791ULL, 2654435761ULL, 2246822519ULL};

struct BuildingPositionKey {
  int64_t EastMm, NorthMm, HeightMm;
  constexpr bool operator==(const BuildingPositionKey &) const = default;
};

struct BuildingPositionHash {
  constexpr uint64_t operator()(const BuildingPositionKey &key) const noexcept {
    return static_cast<uint64_t>(key.EastMm) * kBuildingHashMultipliers[0] ^
           static_cast<uint64_t>(key.NorthMm) * kBuildingHashMultipliers[1] ^
           static_cast<uint64_t>(key.HeightMm) * kBuildingHashMultipliers[2];
  }
};

struct BuildingCornerKey {
  uint32_t Position, Normal, TextureU, TextureV;
  constexpr bool operator==(const BuildingCornerKey &) const = default;
};

struct BuildingCornerHash {
  constexpr uint64_t operator()(const BuildingCornerKey &key) const noexcept {
    return static_cast<uint64_t>(key.Position) * kBuildingHashMultipliers[0] ^
           static_cast<uint64_t>(key.Normal) * kBuildingHashMultipliers[1] ^
           static_cast<uint64_t>(key.TextureU) * kBuildingHashMultipliers[3] ^
           static_cast<uint64_t>(key.TextureV) * kBuildingHashMultipliers[4];
  }
};

struct BuildingScratch final : MeshScratch {
  FlatMap<uint32_t, BuildingPositionKey, BuildingPositionHash> Welded;
  std::array<FlatMap<uint32_t, BuildingCornerKey, BuildingCornerHash>, 2> Corners;

  std::vector<EastNorth> Outline;
  FootprintPiece Whole, Rest, Plot, Beyond, Lo, Hi, Main, Wing, Cap;
  std::array<FootprintPiece, kMaxParts> Row;
  std::vector<double> Side;
  std::vector<int> Sign;
  BuildingShape One, Made;
  Slots<BuildingShape> Parts, Stacked;
  std::vector<EastNorth> Inner;

  std::vector<EastNorth> Overhang, CrownInner, CrownOut, Proud, Foot, Wide, Covered, Tris, Refined;
  std::vector<double> Breaks, Other, At;

  std::vector<uint32_t> Poly;
  Slots<std::vector<EastNorth>> Cells, NextCells;
  std::vector<EastNorth> Mine, Above, Below;

  void ClearWelds() noexcept {
    Welded.Clear();
    Corners[0].Clear();
    Corners[1].Clear();
  }
};

}
#endif
