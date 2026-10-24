#ifndef OUTSHINE_GENERATORS_ROAD_SURFACE_SURFACEBODY_H
#define OUTSHINE_GENERATORS_ROAD_SURFACE_SURFACEBODY_H

#include "SurfaceHeightProgram.h"
#include "HeightBandMesh.h"
#include "HeightSolid.h"
#include "PlanarRegionRemesh.h"
#include <cstddef>
#include <cstdint>
#include <expected>
#include <span>
#include <stop_token>
#include <variant>
#include <vector>

namespace outshine::Generators {
struct SurfaceTreadRecipe {
  uint32_t Source = 0, FirstLevel = 0, Risers = 0;
  double LowM = 0, HighM = 0, RiseM = 0;
  bool operator==(const SurfaceTreadRecipe &) const = default;
};

struct SurfaceBodyBudget {
  static constexpr double kMaximumPlaneErrorM = 1e-8;
  static constexpr double kFlatIntervalM = 1e-10;
  static constexpr double kLandingIntervalM = 1e-7;
  static constexpr double kLevelSelectionBias = 1e-8;
  static constexpr double kDefaultMaximumRiseM = .179;
  static constexpr double kDefaultThicknessM = .3;
  size_t Points = 4096;
  size_t Faces = 8192;
  size_t Flights = 16;
  size_t Risers = 64;
  size_t Visits = 1048576;
  double MaximumRiseM = kDefaultMaximumRiseM;
  double ThicknessM = kDefaultThicknessM;
  PlanarRegionBudget Regions{.Regions = HeightBandBudget{}.Faces};
  HeightBandBudget Bands;
  HeightSolidBudget Solid{.Points = HeightBandBudget{}.Points};
};

struct SurfaceBodyWork {
  size_t Visits = 0;
  size_t NearestTests = 0;
  size_t PlaneTests = 0;
  double MaximumPlaneErrorM = 0;
  PlanarRegionWork Envelope;
  HeightBandWork HeightCuts;
  PlanarRegionWork Bands;
};

struct SurfaceBody {
  std::vector<SurfaceHeightPoint> ReferencePoints;
  std::vector<PlanarFace> ReferenceFaces;
  std::vector<uint64_t> FaceSources;
  std::vector<int32_t> FaceLevels;
  std::vector<double> Levels;
  std::vector<SurfaceTreadRecipe> Stairs;
  HeightSolid Geometry;
  SurfaceBodyWork Work;
};

enum class SurfaceBodyPlanError : uint8_t {
  InvalidInput,
  CapacityExceeded,
  Canceled,
  MissingLanding,
  ConflictingOwners,
  UnsupportedRise,
  CrossingPlane,
  NonplanarFlight
};

using SurfaceBodyError =
    std::variant<SurfaceBodyPlanError, PlanarRegionError, HeightBandError, HeightSolidError>;

[[nodiscard]] std::expected<SurfaceBody, SurfaceBodyError>
BuildSurfaceBody(const SurfaceHeightRequest &request,
                 std::span<const double> heights,
                 SurfaceBodyBudget budget = {},
                 const std::stop_token &stop = {});
}
#endif
