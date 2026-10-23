#ifndef OUTSHINE_GENERATORS_ROAD_SURFACE_SURFACEHEIGHTTOPOLOGYINTERNAL_H
#define OUTSHINE_GENERATORS_ROAD_SURFACE_SURFACEHEIGHTTOPOLOGYINTERNAL_H

#include "SurfaceHeightTopology.h"
#include "PlanHierarchy.h"
#include <array>
#include <cstdint>
#include <expected>
#include <span>
#include <stop_token>
#include <vector>

namespace outshine::Generators::SurfaceHeights {
using Key = std::array<uint32_t, 2>;
using Error = SurfaceHeightTopologyError;
template <typename T> using Checked = std::expected<T, Error>;

struct JoinState {
  std::vector<Key> Keys;
  std::vector<uint32_t> Parents;
  std::vector<uint64_t> PointDomains;
  std::vector<size_t> FaceCounts;
  SurfaceHeightTopologyWork Work;
};

[[nodiscard]] uint32_t Root(std::vector<uint32_t> &parents, uint32_t at);
[[nodiscard]] uint32_t KeyIndex(std::span<const Key> keys, Key key);
[[nodiscard]] Checked<void> JoinPorts(const PlanarArrangement &arrangement,
                                      std::span<const SurfaceHeightPort> ports,
                                      double distanceM,
                                      JoinState &state,
                                      SurfaceHeightTopologyBudget budget,
                                      const std::stop_token &stop);
}
#endif
