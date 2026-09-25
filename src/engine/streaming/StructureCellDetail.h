#ifndef OUTSHINE_ENGINE_STREAMING_STRUCTURECELLDETAIL_H
#define OUTSHINE_ENGINE_STREAMING_STRUCTURECELLDETAIL_H

#include <algorithm>
#include <cmath>

#include "Earth.h"
#include "StructureBake.h"
#include "TileGeodesy.h"
#include "generation/Generate.h"
#include "math/Units.h"
#include "scene/LevelOfDetail.h"

namespace outshine {

[[nodiscard]] inline LevelOfDetail RequestedStructureCellDetail(const Ground::GeoBounds &bounds,
                                                                float maxHeightM,
                                                                double tileSpanM,
                                                                LongitudeLatitude eye,
                                                                double focalPx) noexcept {
  constexpr double kMaxLatitudeDeg = kDegPerHalfTurn / 2.0;
  if (!std::isfinite(bounds.MinLonDeg) || !std::isfinite(bounds.MaxLonDeg) ||
      !std::isfinite(bounds.MinLatDeg) || !std::isfinite(bounds.MaxLatDeg) ||
      !(bounds.MinLonDeg <= bounds.MaxLonDeg) || !(bounds.MinLatDeg <= bounds.MaxLatDeg) ||
      !std::isfinite(maxHeightM) || !(maxHeightM > 0) || !std::isfinite(tileSpanM) ||
      !(tileSpanM > 0) || !std::isfinite(eye.LongitudeDeg) || !std::isfinite(eye.LatitudeDeg) ||
      std::abs(eye.LatitudeDeg) > kMaxLatitudeDeg || !std::isfinite(focalPx) || !(focalPx > 0)) {
    return LevelOfDetail::Fine;
  }
  const double centreLon = 0.5 * (bounds.MinLonDeg + bounds.MaxLonDeg);
  const double localEyeLon = centreLon + std::remainder(eye.LongitudeDeg - centreLon, kDegPerTurn);
  const double nearLat = std::clamp(eye.LatitudeDeg, bounds.MinLatDeg, bounds.MaxLatDeg);
  const double nearLon = std::clamp(localEyeLon, bounds.MinLonDeg, bounds.MaxLonDeg);
  const double lonScale = kMPerDegLon * std::cos(eye.LatitudeDeg * kDeg2Rad);
  const double northM = (eye.LatitudeDeg - nearLat) * kMPerDegLat;
  const double eastM = (localEyeLon - nearLon) * lonScale;
  const double awayM =
      std::max(std::hypot(eastM, northM) - Generators::kStructureEyeDetailGuardM, 1.0);
  const double widthM = (bounds.MaxLonDeg - bounds.MinLonDeg) * lonScale;
  const double depthM = (bounds.MaxLatDeg - bounds.MinLatDeg) * kMPerDegLat;
  const double wholeCellErrorM =
      std::hypot(std::hypot(widthM, depthM), static_cast<double>(maxHeightM)) + 10.0;
  if (!Generators::Unseen(wholeCellErrorM, focalPx, awayM)) { return LevelOfDetail::Fine; }
  const double massedErrorM = wholeCellErrorM + 2.0 * tileSpanM / Generators::kStructureCellSide;
  return Generators::Unseen(massedErrorM, focalPx, awayM) ? LevelOfDetail::Massed
                                                          : LevelOfDetail::Shell;
}

}

#endif
