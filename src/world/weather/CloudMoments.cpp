#include "CloudMoments.h"
#include "CloudIntegration.h"

#include <cmath>

namespace outshine {
namespace {

constexpr double kThinCellDepth = 0.01;
constexpr double kCentroidLinearDivisor = 12.0;
constexpr double kCentroidCubicDivisor = 720.0;
constexpr double kCentroidQuinticDivisor = 30240.0;

double CentroidFraction(double optical) noexcept {
  if (optical < kThinCellDepth) {
    const double square = optical * optical;
    return 0.5 - optical / kCentroidLinearDivisor + optical * square / kCentroidCubicDivisor -
           optical * square * square / kCentroidQuinticDivisor;
  }
  return 1.0 / optical - std::exp(-optical) / -std::expm1(-optical);
}

}

CloudDepthMoments TraceCloudMoments(const CloudLayer &layer,
                                    const Vec3 &originM,
                                    const Vec3 &direction,
                                    const CloudTrace &trace) noexcept {
  CloudDepthMoments result;
  double optical = 0.0;
  VisitCloudCells(layer, originM, direction, trace, [&](const CloudCell &cell) noexcept {
    if (cell.OpticalDepth <= 0.0) { return; }
    const double weight = std::exp(-optical) * -std::expm1(-cell.OpticalDepth);
    const double centroidM = cell.BeginM + cell.LengthM * CentroidFraction(cell.OpticalDepth);
    result.WeightedDistanceM += weight * centroidM;
    optical += cell.OpticalDepth;
  });
  result.Weight = -std::expm1(-optical);
  return result;
}

}
