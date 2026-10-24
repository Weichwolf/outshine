#ifndef OUTSHINE_GENERATORS_ROAD_SURFACE_SURFACELAYOUT_H
#define OUTSHINE_GENERATORS_ROAD_SURFACE_SURFACELAYOUT_H

#include "SurfaceHeightTopology.h"
#include <cstddef>
#include <cstdint>
#include <expected>
#include <optional>
#include <span>
#include <stop_token>
#include <variant>
#include <vector>

namespace outshine::Generators {
struct SurfaceLayoutSource {
  const PlanarDomain &Domain;
  std::span<const PlanarPoint> Axis;
  double WidthM = 0;
  bool Steps = false;
};

struct SurfaceLanding {
  PlanarPoint Origin{};
  std::span<const uint32_t> Sources;
};

struct SurfaceLayoutRecipe {
  static constexpr double kCoordinateMatchM = 1e-8;
  static constexpr double kAreaMatchM2 = 1e-7;
  double LandingReachM = 0;
  std::optional<double> SectionSpacingM = 2;
  PlanarArrangementRecipe Arrangement{.WeldDistanceM = kCoordinateMatchM,
                                      .MaximumAreaDifferenceM2 = kAreaMatchM2};
};

struct SurfaceLayoutBudget {
  size_t Sources = 64;
  size_t Landings = 64;
  size_t InputCoordinates = 4096;
  size_t LandingPairs = 4096;
  size_t SectionsPerSource = 256;
  size_t SectionParts = 1024;
  size_t OutputCoordinates = 65536;
  size_t ConstraintCandidates = PlanarMeshBudget{}.Candidates;
  PlanarArrangementBudget Arrangement{};
};

struct SurfaceSectionRange {
  uint32_t First = 0, Count = 0;
};

struct SurfaceLayoutWork {
  size_t InputCoordinates = 0;
  size_t LandingPairs = 0;
  size_t SectionQueries = 0;
  size_t SectionParts = 0;
  size_t OutputCoordinates = 0;
  size_t CopiedDomainPoints = 0;
  size_t ConstraintQueries = 0;
  size_t ConstraintEdgeTests = 0;
};

struct SurfaceLayout {
  PlanarArrangement Arrangement;
  std::vector<SurfaceHeightPort> Ports;
  std::vector<PlanarLine> Constraints;
  std::vector<SurfaceSectionRange> Sections;
  SurfaceLayoutWork Work;
};

enum class SurfaceLayoutError : uint8_t {
  InvalidInput,
  CapacityExceeded,
  Canceled,
  LibraryFailure,
  DisconnectedLanding,
  ConstraintLost
};

using SurfaceLayoutFailure = std::variant<SurfaceLayoutError, PlanarArrangementError>;

[[nodiscard]] std::expected<SurfaceLayout, SurfaceLayoutFailure>
BuildSurfaceLayout(std::span<const SurfaceLayoutSource> sources,
                   std::span<const SurfaceLanding> landings,
                   SurfaceLayoutRecipe recipe = {},
                   SurfaceLayoutBudget budget = {},
                   const std::stop_token &stop = {});
}
#endif
