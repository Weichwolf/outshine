#ifndef OUTSHINE_GENERATORS_TERRAIN_CORRIDORAPRONBLEND_H
#define OUTSHINE_GENERATORS_TERRAIN_CORRIDORAPRONBLEND_H

#include "GroundMesher.h"
#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

namespace outshine {

class CorridorApronBlend {
public:
  struct Influence {
    double Weight = 0;
    double CorrectionM = 0;
  };

  struct Contribution {
    double Weight = 0;
    double CorrectionM = 0;
    double StrongestWeight = 0;
    uint32_t Which = 0;
  };

  explicit CorridorApronBlend(std::span<const EarthworkStamp> stamps);
  void Reset();
  [[nodiscard]] bool Add(uint32_t stamp, Influence influence);
  [[nodiscard]] Contribution Fold() const;
  [[nodiscard]] size_t HeapBytes() const noexcept;

private:
  std::vector<uint32_t> GroupOf_;
  std::vector<Contribution> Groups_;
  std::vector<uint32_t> Active_;
};

}
#endif
