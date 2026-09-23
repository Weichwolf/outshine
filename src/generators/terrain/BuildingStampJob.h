#ifndef OUTSHINE_GENERATORS_TERRAIN_BUILDINGSTAMPJOB_H
#define OUTSHINE_GENERATORS_TERRAIN_BUILDINGSTAMPJOB_H

#include "BuildingField.h"
#include "GroundMesher.h"
#include "TangentFrame.h"

#include <cstddef>
#include <cstdint>
#include <expected>
#include <optional>
#include <span>
#include <string_view>
#include <vector>

namespace outshine::Generators {

class BuildingStampJob {
public:
  BuildingStampJob(TangentFrame frame, uint64_t vectorGeneration)
      : Frame_(frame), VectorGeneration_(vectorGeneration) {}

  [[nodiscard]] std::expected<bool, std::string_view>
  Advance(std::span<const Ground::BuildingField::Footprint> footprints,
          std::span<const double> points,
          uint64_t vectorGeneration,
          size_t workMost);
  [[nodiscard]] std::expected<std::vector<Yields>, std::string_view> Take() &&;
  [[nodiscard]] size_t HeapBytes() const noexcept;

private:
  enum class Phase : uint8_t { Choose, Ring, Seam, Done };

  TangentFrame Frame_;
  uint64_t VectorGeneration_ = 0;
  std::vector<Yields> Stamps_;
  std::optional<Yields> Current_;
  std::optional<size_t> FootprintCount_;
  std::optional<size_t> PointCount_;
  size_t NextFootprint_ = 0;
  size_t NextPoint_ = 0;
  size_t NextSeam_ = 0;
  Phase Phase_ = Phase::Choose;
};

}
#endif
