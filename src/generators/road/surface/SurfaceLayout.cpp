#include "SurfaceLayoutInternal.h"
#include "GeosPolygon.h"
#include <geos_c.h>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <limits>
#include <span>
#include <stop_token>
#include <utility>
#include <vector>

namespace outshine::Generators::SurfaceLayouts {
Context::Context(const std::stop_token &stop) : Stop(stop) {
#if GEOS_VERSION_MAJOR > 3 || GEOS_VERSION_MINOR >= 14
  if (Handle != nullptr) {
    GEOSContext_setInterruptCallback_r(
        Handle,
        [](void *data) {
          return static_cast<const Context *>(data)->Stop.stop_requested() ? 1 : 0;
        },
        this);
  }
#endif
}

Context::~Context() {
  if (Handle != nullptr) { GEOS_finish_r(Handle); }
}

Geometry Line(const Context &context, std::span<const PlanarPoint> points) {
  auto *coordinates =
      GEOSCoordSeq_create_r(context.Handle, static_cast<unsigned>(points.size()), 2);
  if (coordinates == nullptr) { return context.Own(nullptr); }
  for (size_t at = 0; at < points.size(); ++at) {
    if (GEOSCoordSeq_setXY_r(
            context.Handle, coordinates, static_cast<unsigned>(at), points[at][0], points[at][1]) ==
        0) {
      GEOSCoordSeq_destroy_r(context.Handle, coordinates);
      return context.Own(nullptr);
    }
  }
  return context.Own(GEOSGeom_createLineString_r(context.Handle, coordinates));
}

Checked<PlanarLine> Coordinates(const Context &context,
                                const GEOSGeometry *line,
                                SurfaceLayout &output,
                                SurfaceLayoutBudget budget) {
  const auto *coordinates = GEOSGeom_getCoordSeq_r(context.Handle, line);
  unsigned count = 0;
  if (coordinates == nullptr || GEOSCoordSeq_getSize_r(context.Handle, coordinates, &count) == 0) {
    return std::unexpected(context.Failure());
  }
  if (count > budget.OutputCoordinates - output.Work.OutputCoordinates) {
    return std::unexpected(Error::CapacityExceeded);
  }
  PlanarLine result(count);
  for (unsigned at = 0; at < count; ++at) {
    if (context.Stop.stop_requested()) { return std::unexpected(Error::Canceled); }
    if (GEOSCoordSeq_getXY_r(context.Handle, coordinates, at, result[at].data(), &result[at][1]) ==
            0 ||
        !std::isfinite(result[at][0]) || !std::isfinite(result[at][1])) {
      return std::unexpected(context.Failure());
    }
  }
  output.Work.OutputCoordinates += count;
  return result;
}

namespace {
Checked<void> InputLine(std::span<const PlanarPoint> points,
                        size_t minimum,
                        SurfaceLayoutBudget budget,
                        SurfaceLayout &output) {
  if (points.size() < minimum) { return std::unexpected(Error::InvalidInput); }
  if (points.size() > budget.InputCoordinates - output.Work.InputCoordinates) {
    return std::unexpected(Error::CapacityExceeded);
  }
  output.Work.InputCoordinates += points.size();
  const double bound = std::sqrt(std::numeric_limits<double>::max()) / 8;
  for (const auto &point : points) {
    if (!std::isfinite(point[0]) || !std::isfinite(point[1]) || std::abs(point[0]) > bound ||
        std::abs(point[1]) > bound) {
      return std::unexpected(Error::InvalidInput);
    }
  }
  return {};
}

Checked<void>
InputSource(const SurfaceLayoutSource &source, SurfaceLayoutBudget budget, SurfaceLayout &output) {
  constexpr double kMaximumWidthM = 30;
  if (!std::isfinite(source.WidthM) || source.WidthM <= 0 || source.WidthM > kMaximumWidthM ||
      source.Domain.Rings.empty()) {
    return std::unexpected(Error::InvalidInput);
  }
  const auto axis = InputLine(source.Axis, 2, budget, output);
  if (!axis) { return std::unexpected(axis.error()); }
  for (const auto &ring : source.Domain.Rings) {
    const auto valid = InputLine(ring, 3, budget, output);
    if (!valid) { return std::unexpected(valid.error()); }
  }
  return {};
}

struct SourceCoverage {
  const GEOSGeometry *Domain;
  const GEOSGeometry *Axis;
};

Checked<void> ValidateCoverage(const Context &context, SourceCoverage source) {
  const char valid = GEOSisValid_r(context.Handle, source.Domain);
  if (valid == 2) { return std::unexpected(context.Failure()); }
  if (valid != 1) { return std::unexpected(Error::InvalidInput); }
  const char covered = GEOSCovers_r(context.Handle, source.Domain, source.Axis);
  if (covered == 2) { return std::unexpected(context.Failure()); }
  if (covered == 1) { return {}; }
  constexpr int kCoverageQuadrants = 8;
  const auto numericRegion = context.Own(GEOSBuffer_r(
      context.Handle, source.Domain, SurfaceLayoutRecipe::kCoordinateMatchM, kCoverageQuadrants));
  if (!numericRegion) { return std::unexpected(context.Failure()); }
  const char withinBound = GEOSCovers_r(context.Handle, numericRegion.get(), source.Axis);
  if (withinBound == 2) { return std::unexpected(context.Failure()); }
  if (withinBound != 1) { return std::unexpected(Error::InvalidInput); }
  return {};
}

Checked<Sources> MakeSources(const Context &context,
                             std::span<const SurfaceLayoutSource> sources,
                             SurfaceLayoutBudget budget,
                             SurfaceLayout &output) {
  Sources result;
  result.Domains.reserve(sources.size());
  result.Axes.reserve(sources.size());
  for (const auto &source : sources) {
    if (context.Stop.stop_requested()) { return std::unexpected(Error::Canceled); }
    const auto input = InputSource(source, budget, output);
    if (!input) { return std::unexpected(input.error()); }
    auto polygon = Spatial::MakeGeosPolygon(context.Handle, source.Domain.Rings);
    auto line = Line(context, source.Axis);
    if (!polygon || !line) { return std::unexpected(context.Failure()); }
    const auto covered = ValidateCoverage(context, {.Domain = polygon.get(), .Axis = line.get()});
    if (!covered) { return std::unexpected(covered.error()); }
    double length = 0;
    if (GEOSLength_r(context.Handle, line.get(), &length) == 0) {
      return std::unexpected(context.Failure());
    }
    if (!std::isfinite(length) || length <= 0) { return std::unexpected(Error::InvalidInput); }
    result.Domains.push_back(std::move(polygon));
    result.Axes.push_back(std::move(line));
  }
  return result;
}
}
}

namespace outshine::Generators {
std::expected<SurfaceLayout, SurfaceLayoutFailure>
BuildSurfaceLayout(std::span<const SurfaceLayoutSource> sources,
                   std::span<const SurfaceLanding> landings,
                   SurfaceLayoutRecipe recipe,
                   SurfaceLayoutBudget budget,
                   const std::stop_token &stop) {
  using namespace SurfaceLayouts;
  using Failure = SurfaceLayoutFailure;
  if (stop.stop_requested()) { return std::unexpected(Failure{Error::Canceled}); }
  if (sources.size() > budget.Sources || landings.size() > budget.Landings ||
      sources.size() > std::numeric_limits<uint64_t>::digits ||
      budget.InputCoordinates >= std::numeric_limits<unsigned>::max() ||
      budget.OutputCoordinates >= std::numeric_limits<uint32_t>::max()) {
    return std::unexpected(Failure{Error::CapacityExceeded});
  }
  constexpr double kMaximumReachM = 2;
  if (sources.empty() || !std::isfinite(recipe.LandingReachM) || recipe.LandingReachM < 0 ||
      recipe.LandingReachM > kMaximumReachM) {
    return std::unexpected(Failure{Error::InvalidInput});
  }
  SurfaceLayout output;
  const Context context(stop);
  if (context.Handle == nullptr) { return std::unexpected(Failure{Error::LibraryFailure}); }
  const auto geometries = MakeSources(context, sources, budget, output);
  if (!geometries) { return std::unexpected(Failure{geometries.error()}); }
  const auto ports = Landings(context, sources, landings, *geometries, recipe, budget, output);
  if (!ports) { return std::unexpected(Failure{ports.error()}); }
  const auto sections = Sections(context, sources, *geometries, recipe, budget, output);
  if (!sections) { return std::unexpected(Failure{sections.error()}); }
  std::vector<PlanarDomain> domains;
  domains.reserve(sources.size());
  for (const auto &source : sources) {
    for (const auto &ring : source.Domain.Rings) { output.Work.CopiedDomainPoints += ring.size(); }
    domains.push_back(source.Domain);
  }
  const auto lines = NodingLines(output, recipe.Arrangement.WeldDistanceM, stop);
  if (!lines) { return std::unexpected(Failure{lines.error()}); }
  auto arrangement =
      ArrangePlanarDomains(domains, *lines, recipe.Arrangement, budget.Arrangement, stop);
  if (!arrangement) { return std::unexpected(Failure{arrangement.error()}); }
  output.Arrangement = std::move(*arrangement);
  const auto conformed = ValidateSections(output, recipe.Arrangement.WeldDistanceM, budget, stop);
  if (!conformed) { return std::unexpected(Failure{conformed.error()}); }
  return output;
}
}
