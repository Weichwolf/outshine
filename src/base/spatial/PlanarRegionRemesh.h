#ifndef OUTSHINE_BASE_SPATIAL_PLANARREGIONREMESH_H
#define OUTSHINE_BASE_SPATIAL_PLANARREGIONREMESH_H

#include "PlanarMesh.h"
#include <cstddef>
#include <cstdint>
#include <expected>
#include <span>
#include <stop_token>
#include <vector>

namespace outshine {
using PlanarFaceRegion = std::vector<uint32_t>;

struct PlanarRegionBudget {
  static constexpr size_t kDefaultFaces = 8192;
  static constexpr size_t kDefaultRingVertices = 3 * kDefaultFaces;
  static constexpr double kMaximumCollinearityM = 1e-10;
  static constexpr double kMaximumCoverageM2 = 1e-7;
  size_t Points = 65536;
  size_t Faces = kDefaultFaces;
  size_t Regions = 1024;
  size_t RingVertices = kDefaultRingVertices;
  size_t RingVisits = 8 * kDefaultRingVertices;
  double CollinearityM = kMaximumCollinearityM;
  double CoverageM2 = kMaximumCoverageM2;
};

struct PlanarRegionWork {
  size_t InputFaces = 0;
  size_t OutputFaces = 0;
  size_t RingVisits = 0;
  size_t RemovedVertices = 0;
  double MaximumChainDeviationM = 0;
  double CoverageErrorM2 = 0;
};

struct PlanarRegionMesh {
  std::vector<std::vector<PlanarFace>> Regions;
  PlanarRegionWork Work;
};

enum class PlanarRegionError : uint8_t {
  InvalidInput,
  CapacityExceeded,
  Canceled,
  LibraryFailure,
  AmbiguousVertices,
  CoverageChanged
};

[[nodiscard]] std::expected<PlanarRegionMesh, PlanarRegionError>
RemeshPlanarRegions(std::span<const PlanarPoint> points,
                    std::span<const PlanarFace> faces,
                    std::span<const PlanarFaceRegion> regions,
                    PlanarRegionBudget budget = {},
                    const std::stop_token &stop = {});
}
#endif
