#include "HeightSolidTerrainContact.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <expected>
#include <utility>

namespace outshine::Generators {
bool ValidSurfaceGroundSource(const SurfaceGroundSource &source) noexcept {
  if (!std::isfinite(source.ClearanceM) || source.ClearanceM < 0 ||
      !std::isfinite(source.ReliefM) || source.ReliefM < 0) {
    return false;
  }
  switch (source.Effect) {
    case SurfaceGroundEffect::GroundContact: return source.ContactKey != 0;
    case SurfaceGroundEffect::OverheadClearance:
    case SurfaceGroundEffect::NoEarthwork: return true;
    case SurfaceGroundEffect::Unspecified: return false;
  }
  return false;
}

namespace {
using Error = SurfaceEarthworkError;
template <typename T> using Checked = std::expected<T, Error>;

Checked<std::array<std::array<double, 3>, 3>> Bottom(const HeightSolid &body, size_t face) {
  const auto bottom = body.Faces[body.SurfaceFaces + face];
  std::array<std::array<double, 3>, 3> result{};
  for (size_t at = 0; at < bottom.size(); ++at) {
    const auto index = bottom[bottom.size() - 1 - at];
    if (index >= body.Points.size()) { return std::unexpected(Error::InvalidInput); }
    result[at] = body.Points[index];
    for (const double value : result[at]) {
      if (!std::isfinite(value)) { return std::unexpected(Error::InvalidInput); }
    }
  }
  return result;
}

Checked<EarthworkStamp> Stamp(const std::array<std::array<double, 3>, 3> &points,
                              const SurfaceGroundSource &source,
                              double apronM,
                              EarthworkStamp storage) {
  const auto origin = points[0];
  const double aE = points[1][0] - origin[0];
  const double aN = points[1][1] - origin[1];
  const double aZ = points[1][2] - origin[2];
  const double bE = points[2][0] - origin[0];
  const double bN = points[2][1] - origin[1];
  const double bZ = points[2][2] - origin[2];
  const double determinant = aE * bN - aN * bE;
  if (!std::isfinite(determinant) || determinant <= 0) {
    return std::unexpected(Error::InvalidInput);
  }
  EarthworkStamp stamp;
  stamp.RingEastNorthM = std::move(storage.RingEastNorthM);
  stamp.SeamEastNorthM = std::move(storage.SeamEastNorthM);
  stamp.RingEastNorthM.clear();
  stamp.SeamEastNorthM.clear();
  stamp.LowE = stamp.HighE = stamp.AtE = origin[0];
  stamp.LowN = stamp.HighN = stamp.AtN = origin[1];
  stamp.PlateauM = origin[2];
  stamp.SlopeE = (aZ * bN - aN * bZ) / determinant;
  stamp.SlopeN = (aE * bZ - aZ * bE) / determinant;
  stamp.CorridorKey = source.ContactKey;
  stamp.ApronM = apronM;
  stamp.YieldM = source.ReliefM;
  stamp.PlanarSupportM = 0;
  stamp.Fills = source.Effect == SurfaceGroundEffect::GroundContact;
  stamp.Kind = stamp.Fills ? EarthworkKind::Corridor : EarthworkKind::Clearance;
  if (!stamp.Fills) { stamp.PlateauM -= source.ClearanceM; }
  if (!std::isfinite(stamp.PlateauM) || !std::isfinite(stamp.SlopeE) ||
      !std::isfinite(stamp.SlopeN)) {
    return std::unexpected(Error::InvalidInput);
  }
  stamp.RingEastNorthM.reserve(points.size() * 2);
  for (const auto point : points) {
    stamp.LowE = std::min(stamp.LowE, point[0]);
    stamp.HighE = std::max(stamp.HighE, point[0]);
    stamp.LowN = std::min(stamp.LowN, point[1]);
    stamp.HighN = std::max(stamp.HighN, point[1]);
    stamp.RingEastNorthM.push_back(point[0]);
    stamp.RingEastNorthM.push_back(point[1]);
  }
  stamp.SeamEastNorthM = stamp.RingEastNorthM;
  return stamp;
}
}

std::expected<EarthworkStamp, SurfaceEarthworkError>
BuildHeightSolidTerrainContact(const HeightSolid &body,
                               size_t face,
                               const SurfaceGroundSource &source,
                               double apronM,
                               EarthworkStamp storage) {
  if (face >= body.SurfaceFaces || body.SurfaceFaces > body.Faces.size() / 2 ||
      !ValidSurfaceGroundSource(source) || source.Effect == SurfaceGroundEffect::NoEarthwork ||
      !std::isfinite(apronM) || apronM < 0) {
    return std::unexpected(SurfaceEarthworkError::InvalidInput);
  }
  const auto bottom = Bottom(body, face);
  if (!bottom) { return std::unexpected(bottom.error()); }
  return Stamp(*bottom, source, apronM, std::move(storage));
}
}
