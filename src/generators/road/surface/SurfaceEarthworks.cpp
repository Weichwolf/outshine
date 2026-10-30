#include "SurfaceEarthworks.h"
#include <algorithm>
#include <bit>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <span>
#include <stop_token>
#include <utility>

namespace outshine::Generators {
namespace {
using Error = SurfaceEarthworkError;
template <typename T> using Checked = std::expected<T, Error>;

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
      !std::isfinite(budget.ApronM) || budget.ApronM < 0 ||
      !std::ranges::all_of(sources, ValidSurfaceGroundSource)) {
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
    auto stamp = BuildHeightSolidTerrainContact(body.Geometry, at, *source, budget.ApronM);
    if (!stamp) { return std::unexpected(stamp.error()); }
    output.Stamps.push_back(std::move(*stamp));
    output.FaceParents.push_back(at);
  }
  return output;
}
}
