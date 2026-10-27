#ifndef OUTSHINE_GENERATORS_TERRAIN_EARTHWORKSTAMPINDEX_H
#define OUTSHINE_GENERATORS_TERRAIN_EARTHWORKSTAMPINDEX_H

#include "Earth.h"
#include "EarthworkStamp.h"
#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

namespace outshine {

struct EarthworkStampQuery {
  std::vector<uint32_t> Candidates;
  std::vector<uint32_t> CoarseCandidates;

  [[nodiscard]] size_t HeapBytes() const noexcept {
    return (Candidates.capacity() + CoarseCandidates.capacity()) * sizeof(uint32_t);
  }
};

class EarthworkStampIndex {
public:
  EarthworkStampIndex(std::span<const EarthworkStamp> stamps, std::span<const double> widthsM);

  [[nodiscard]] std::span<const uint32_t> At(EastNorth point, EarthworkStampQuery &scratch) const;
  [[nodiscard]] size_t HeapBytes() const noexcept;

private:
  struct CellBounds {
    int64_t LowE = 0, HighE = 0, LowN = 0, HighN = 0;
    uint32_t Stamp = 0;
  };

  struct CellLevel {
    explicit CellLevel(uint32_t shift = 0) : Shift(shift) {}

    void Add(const CellBounds &bounds, uint32_t stamp);
    void Build();
    void BuildSparse();
    [[nodiscard]] size_t CellOffset(uint64_t key) const;
    [[nodiscard]] std::span<const uint32_t> At(std::array<int64_t, 2> cell) const;
    [[nodiscard]] size_t HeapBytes() const noexcept;

    uint32_t Shift = 0;
    int64_t LowE = 0, LowN = 0, ColumnCount = 0;
    std::vector<uint64_t> CellKeys;
    std::vector<uint32_t> CellOffsets;
    std::vector<uint32_t> StampIndices;
  };

  [[nodiscard]] static uint32_t ShiftFor(const CellBounds &bounds);

  CellLevel Base_;
  std::vector<CellLevel> CoarseLevels_;
  std::vector<CellBounds> CoarseSupports_;
};

}
#endif
