#ifndef OUTSHINE_GENERATORS_ROAD_SURFACE_SURFACEHEIGHTTOPOLOGY_H
#define OUTSHINE_GENERATORS_ROAD_SURFACE_SURFACEHEIGHTTOPOLOGY_H

#include "PlanarArrangement.h"
#include <cstddef>
#include <cstdint>
#include <expected>
#include <span>
#include <stop_token>
#include <vector>

namespace outshine::Generators {
struct SurfaceHeightPort {
  PlanarDomain Region;
  std::vector<uint32_t> Domains;
};

struct SurfaceHeightTopologyBudget {
  size_t PlanarVertices = 4096;
  size_t SourceVertices = 4096;
  size_t SourceFaces = 8192;
  size_t PortPoints = 4096;
  size_t PortCandidates = 65536;
};

struct SurfaceHeightFaces {
  std::vector<PlanarFace> Faces;
  std::vector<uint32_t> PlanarFaces;
};

struct SurfaceHeightTopologyWork {
  size_t SourceVertices = 0;
  size_t PortCandidates = 0;
  size_t Joins = 0;
  size_t IndexHeapBytes = 0;
};

struct SurfaceHeightTopology {
  std::vector<PlanarPoint> Points;
  std::vector<uint32_t> PlanarVertices;
  std::vector<uint32_t> PrimaryDomains;
  std::vector<SurfaceHeightFaces> Sources;
  SurfaceHeightTopologyWork Work;
};

enum class SurfaceHeightTopologyError : uint8_t {
  InvalidInput,
  CapacityExceeded,
  Canceled,
  LibraryFailure
};

[[nodiscard]] std::expected<SurfaceHeightTopology, SurfaceHeightTopologyError>
BuildSurfaceHeightTopology(const PlanarArrangement &arrangement,
                           uint32_t domainCount,
                           std::span<const SurfaceHeightPort> ports,
                           double joinDistanceM,
                           SurfaceHeightTopologyBudget budget = {},
                           const std::stop_token &stop = {});
}
#endif
