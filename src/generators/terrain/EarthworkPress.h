#ifndef OUTSHINE_GENERATORS_TERRAIN_EARTHWORKPRESS_H
#define OUTSHINE_GENERATORS_TERRAIN_EARTHWORKPRESS_H

#include <cstddef>
#include <cstdint>
#include <memory>
#include <span>
#include <vector>

#include "GroundMesher.h"

namespace outshine {

inline constexpr uint32_t kNoStamp = 0xFFFFFFFFu;
inline constexpr uint32_t kHeldStamp = 0xFFFFFFFEu;

struct EarthworkPointClaim {
  uint32_t Point = 0;
  uint32_t Stamp = 0;
};

struct EarthworkHeightView {
  std::span<const double> WrittenM;
  std::span<const double> WasM;
};

struct EarthworkMetrics {
  size_t Stamps = 0;
  size_t Unreached = 0;
  size_t Nodes = 0;
  size_t Contested = 0;
  double AboveM = 0.0;
  double BelowM = 0.0;
  double UnfilledM = 0.0;
  double WasAboveM = 0.0;
  double WasBelowM = 0.0;
};

struct EarthworkPressResult {
  size_t Moved = 0;
  size_t Structures = 0;
  size_t Held = 0;
  std::vector<uint8_t> Refused;
  std::vector<uint32_t> DecidedBy;
  std::vector<EarthworkPointClaim> Inside;
  double BucketMs = 0.0;
  double RejectMs = 0.0;
  double ApplyMs = 0.0;
  double LongestRejectMs = 0.0;
  double LongestInitializeMs = 0.0;
  double LongestApplyMs = 0.0;
};

[[nodiscard]] EarthworkPressResult ApplyEarthworkStamps(std::span<const EarthworkStamp> these,
                                                        std::span<const EastNorth> at,
                                                        std::span<double> upM,
                                                        double mostEarthworkM);

class EarthworkPressJob {
public:
  EarthworkPressJob(std::span<const EarthworkStamp> these,
                    std::span<const EastNorth> at,
                    std::span<double> upM,
                    double mostEarthworkM);
  ~EarthworkPressJob();
  EarthworkPressJob(const EarthworkPressJob &) = delete;
  EarthworkPressJob &operator=(const EarthworkPressJob &) = delete;
  EarthworkPressJob(EarthworkPressJob &&) noexcept;
  EarthworkPressJob &operator=(EarthworkPressJob &&) noexcept;

  [[nodiscard]] bool Advance(size_t pointsMost);
  [[nodiscard]] EarthworkPressResult Take() noexcept;
  [[nodiscard]] size_t HeapBytes() const noexcept;

private:
  struct State;
  std::unique_ptr<State> State_;
};

[[nodiscard]] EarthworkMetrics MeasureEarthworkEffect(std::span<const EarthworkStamp> these,
                                                      const EarthworkPressResult &pressed,
                                                      EarthworkKind kind,
                                                      std::span<const EastNorth> at,
                                                      EarthworkHeightView heights);

}
#endif
