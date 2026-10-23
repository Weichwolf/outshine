#ifndef OUTSHINE_EXPERIMENT_RECEIVER_BLOCK_RECEIVER_H
#define OUTSHINE_EXPERIMENT_RECEIVER_BLOCK_RECEIVER_H
#include "Projection.h"
#include "receiver_coverage/Coverage.h"
#include <array>
#include <cstdint>
#include <span>

namespace ReceiverBlocks {
struct Camera {
  Matrix Inverse;
  std::array<double, 3> Eye;
  uint32_t Width;
  uint32_t Height;
};

struct Region {
  Matrix Clip;
  uint32_t X;
  uint32_t Y;
};

struct Costs {
  uint64_t Nodes = 0;
  uint64_t Projections = 0;
  uint64_t Points = 0;
  uint64_t DepthReads = 0;
  uint64_t SourceSamples = 0;
  uint64_t Refined = 0;
  uint64_t Covered = 0;
  uint64_t CoversCalls = 0;
  uint64_t WordUpdates = 0;
  uint64_t MarkedCellArea = 0;
  uint64_t Unknown = 0;
  uint64_t Modeled = 0;
};

[[nodiscard]] bool Append(const Camera &camera,
                          std::span<const float> depth,
                          std::span<const Region> regions,
                          uint32_t atlasSide,
                          ReceiverCoverage::RowMask &mask,
                          Costs &costs) noexcept;
}
#endif
