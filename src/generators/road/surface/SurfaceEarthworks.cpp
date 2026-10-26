#include "SurfaceEarthworks.h"
#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <optional>
#include <span>
#include <stop_token>
#include <utility>

namespace outshine::Generators {
namespace {
using Error = SurfaceEarthworkError;
template <typename T> using Checked = std::expected<T, Error>;

bool Valid(const SurfaceGroundSource &source) {
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

Checked<SurfaceGroundSource> Resolve(uint64_t mask, std::span<const SurfaceGroundSource> sources) {
  if (mask == 0 || (sources.size() < 64 && (mask >> sources.size()) != 0)) {
    return std::unexpected(Error::InvalidInput);
  }
  SurfaceGroundSource result{.Effect = SurfaceGroundEffect::NoEarthwork};
  while (mask != 0) {
    const auto source = sources[static_cast<size_t>(std::countr_zero(mask))];
    mask &= mask - 1;
    if (source.Effect == SurfaceGroundEffect::GroundContact) {
      if (result.Effect == SurfaceGroundEffect::GroundContact &&
          result.ContactKey != source.ContactKey) {
        return std::unexpected(Error::ConflictingContactKeys);
      }
      if (result.Effect != SurfaceGroundEffect::GroundContact) { result = source; }
      result.ReliefM = std::max(result.ReliefM, source.ReliefM);
    } else if (source.Effect == SurfaceGroundEffect::OverheadClearance &&
               result.Effect != SurfaceGroundEffect::GroundContact) {
      result.Effect = source.Effect;
      result.ClearanceM = std::max(result.ClearanceM, source.ClearanceM);
      result.ReliefM = std::max(result.ReliefM, source.ReliefM);
    }
  }
  return result;
}

Checked<std::array<SurfaceHeightPoint, 3>> Bottom(const SurfaceBody &body, size_t face) {
  const auto bottom = body.Geometry.Faces[body.Geometry.SurfaceFaces + face];
  std::array<SurfaceHeightPoint, 3> result{};
  for (size_t at = 0; at < bottom.size(); ++at) {
    const auto index = bottom[bottom.size() - 1 - at];
    if (index >= body.Geometry.Points.size()) { return std::unexpected(Error::InvalidInput); }
    result[at] = body.Geometry.Points[index];
    for (const double value : result[at]) {
      if (!std::isfinite(value)) { return std::unexpected(Error::InvalidInput); }
    }
  }
  return result;
}

Checked<EarthworkStamp> Stamp(const std::array<SurfaceHeightPoint, 3> &points,
                              const SurfaceGroundSource &source,
                              double apronM) {
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

std::expected<SurfaceEarthworks, SurfaceEarthworkError>
BuildSurfaceEarthworks(const SurfaceBody &body,
                       std::span<const SurfaceGroundSource> sources,
                       SurfaceEarthworkBudget budget,
                       const std::stop_token &stop) {
  if (stop.stop_requested()) { return std::unexpected(Error::Canceled); }
  if (body.ReferenceFaces.size() > budget.Faces || sources.size() > 64) {
    return std::unexpected(Error::CapacityExceeded);
  }
  if (body.ReferenceFaces.size() != body.Geometry.SurfaceFaces ||
      body.FaceSources.size() != body.ReferenceFaces.size() ||
      body.Geometry.SurfaceFaces > body.Geometry.Faces.size() / 2 ||
      !std::isfinite(budget.ApronM) || budget.ApronM < 0 || !std::ranges::all_of(sources, Valid)) {
    return std::unexpected(Error::InvalidInput);
  }
  SurfaceEarthworks output;
  if (std::ranges::any_of(sources, [](const SurfaceGroundSource &source) {
        return source.Effect != SurfaceGroundEffect::NoEarthwork;
      })) {
    output.Stamps.reserve(body.ReferenceFaces.size());
    output.FaceParents.reserve(body.ReferenceFaces.size());
  }
  for (uint32_t at = 0; at < body.ReferenceFaces.size(); ++at) {
    if (stop.stop_requested()) { return std::unexpected(Error::Canceled); }
    const auto source = Resolve(body.FaceSources[at], sources);
    if (!source) { return std::unexpected(source.error()); }
    if (source->Effect == SurfaceGroundEffect::NoEarthwork) {
      ++output.NoEarthworkFaces;
      continue;
    }
    const auto bottom = Bottom(body, at);
    if (!bottom) { return std::unexpected(bottom.error()); }
    auto stamp = Stamp(*bottom, *source, budget.ApronM);
    if (!stamp) { return std::unexpected(stamp.error()); }
    output.Stamps.push_back(std::move(*stamp));
    output.FaceParents.push_back(at);
  }
  return output;
}
}
