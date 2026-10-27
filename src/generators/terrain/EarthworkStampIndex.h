#ifndef OUTSHINE_GENERATORS_TERRAIN_EARTHWORKSTAMPINDEX_H
#define OUTSHINE_GENERATORS_TERRAIN_EARTHWORKSTAMPINDEX_H

#include "Earth.h"
#include "EarthworkStamp.h"
#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

namespace outshine {

class EarthworkStampIndex {
public:
  EarthworkStampIndex(std::span<const EarthworkStamp> stamps, std::span<const double> widthsM);

  [[nodiscard]] std::span<const uint32_t> At(EastNorth point) const;
  [[nodiscard]] size_t HeapBytes() const noexcept;

private:
  [[nodiscard]] static int64_t CellOf(double metres);
  [[nodiscard]] static uint64_t KeyOf(int64_t east, int64_t north);
  [[nodiscard]] size_t SeatOf(uint64_t key) const;
  void Spread(EastNorth low, EastNorth high, uint32_t stamp);
  void Build();
  void BuildSparse();

  int64_t LowE_ = 0;
  int64_t LowN_ = 0;
  int64_t Wide_ = 0;
  std::vector<uint64_t> Keys_;
  std::vector<uint32_t> First_;
  std::vector<uint32_t> Seats_;
};

}
#endif
