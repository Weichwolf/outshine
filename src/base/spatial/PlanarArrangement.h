#ifndef OUTSHINE_BASE_SPATIAL_PLANARARRANGEMENT_H
#define OUTSHINE_BASE_SPATIAL_PLANARARRANGEMENT_H

#include "PlanarMesh.h"
#include "PolygonTriangulation.h"
#include <cstddef>
#include <cstdint>
#include <expected>
#include <span>
#include <stop_token>
#include <vector>

namespace outshine {
using PlanarLine = std::vector<PlanarPoint>;

struct PlanarDomain {
  std::vector<PolygonRing> Rings;
};

struct PlanarArrangementRecipe {
  double WeldDistanceM = 0;
  double MaximumAreaDifferenceM2 = 0;
};

struct PlanarArrangementBudget {
  PlanarMeshBudget Mesh;
  size_t InputSegments = 4096;
  size_t NodedSegments = 8192;
  size_t Cells = 8192;
  size_t DomainTests = 524288;
};

struct PlanarArrangementWork {
  size_t InputSegments = 0;
  size_t NodedSegments = 0;
  size_t Cells = 0;
  size_t DomainTests = 0;
  double MaximumAreaDifferenceM2 = 0;
};

struct PlanarArrangement {
  PlanarMesh Mesh;
  std::vector<uint64_t> FaceDomains;
  PlanarArrangementWork Work;
};

enum class PlanarArrangementError : uint8_t {
  InvalidInput,
  CapacityExceeded,
  Canceled,
  LibraryFailure,
  FootprintChanged,
  WeldDistanceExceeded
};

[[nodiscard]] std::expected<PlanarArrangement, PlanarArrangementError>
ArrangePlanarDomains(std::span<const PlanarDomain> domains,
                     std::span<const PlanarLine> constraints,
                     PlanarArrangementRecipe recipe,
                     PlanarArrangementBudget budget = {},
                     const std::stop_token &stop = {});
}
#endif
