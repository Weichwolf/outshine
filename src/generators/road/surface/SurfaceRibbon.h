#ifndef OUTSHINE_GENERATORS_ROAD_SURFACE_SURFACERIBBON_H
#define OUTSHINE_GENERATORS_ROAD_SURFACE_SURFACERIBBON_H

#include "PlanarArrangement.h"
#include <cstddef>
#include <cstdint>
#include <expected>
#include <span>
#include <stop_token>
#include <vector>

namespace outshine::Generators {
struct SurfaceRibbonEnd {
  double StationM = 0;
  std::span<const PlanarPoint> Profile;
  size_t AxisVertex = 0;
};

struct SurfaceRibbonBudget {
  static constexpr size_t kDefaultAxisPoints = 256;
  static constexpr size_t kDefaultProfilePoints = 128;
  static constexpr double kMaximumProfileErrorM = 1e-7;
  static constexpr double kMinimumMiterDenominator = .25;
  size_t AxisPoints = kDefaultAxisPoints;
  size_t ProfilePoints = kDefaultProfilePoints;
  size_t RingPoints = 2 * (kDefaultAxisPoints + kDefaultProfilePoints);
};

struct SurfaceRibbon {
  std::vector<PlanarPoint> Axis;
  PlanarDomain Domain;
  double AreaM2 = 0;
  double MaximumMiter = 0;
};

enum class SurfaceRibbonError : uint8_t {
  InvalidInput,
  CapacityExceeded,
  Canceled,
  InvalidProfile,
  UnsupportedTurn,
  InvalidFootprint,
  LibraryFailure
};

[[nodiscard]] std::expected<SurfaceRibbon, SurfaceRibbonError>
BuildSurfaceRibbon(std::span<const PlanarPoint> originalAxis,
                   SurfaceRibbonEnd first,
                   SurfaceRibbonEnd last,
                   SurfaceRibbonBudget budget = {},
                   const std::stop_token &stop = {});
}
#endif
