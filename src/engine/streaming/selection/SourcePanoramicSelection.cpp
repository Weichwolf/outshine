#include "SourceProductSelection.h"
#include "Box.h"
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <limits>
#include <numbers>
#include <optional>

namespace outshine {
namespace {
struct ErrorEnvelope {
  double WorldM = 0;
  double BoundsExpansionM = 0;
};

struct PanoramicError {
  double DepthScale = 0;
  double DisplacementScalePx = 0;

  [[nodiscard]] double Bound(double distance, ErrorEnvelope envelope) const {
    if (envelope.WorldM == 0) { return 0; }
    if (!std::isfinite(distance)) { return std::numeric_limits<double>::infinity(); }
    const double depth = std::max(0.0, distance - envelope.BoundsExpansionM) / DepthScale;
    return depth > envelope.WorldM
               ? DisplacementScalePx * envelope.WorldM / (depth - envelope.WorldM)
               : std::numeric_limits<double>::infinity();
  }

  [[nodiscard]] double Limit(double distance, double pixels) const {
    const double infinity = std::numeric_limits<double>::infinity();
    const double projected = std::nextafter(DisplacementScalePx * DepthScale, infinity);
    const double expansion =
        std::nextafter(pixels * std::nextafter(DepthScale + 1, infinity), infinity);
    const double denominator = std::nextafter(projected + expansion, infinity);
    if (!std::isfinite(denominator) || denominator <= 0 || !std::isfinite(distance) ||
        distance <= 0) {
      return 0;
    }
    const double fraction = std::nextafter(pixels / denominator, 0.0);
    const double error = std::nextafter(fraction * distance, 0.0);
    return error > 0 && std::isfinite(error) &&
                   Bound(distance, {.WorldM = error, .BoundsExpansionM = error}) <= pixels
               ? error
               : 0;
  }
};

PanoramicError Envelope(const Vec2 &viewport, double focal) {
  const double tx = viewport[0] / (2 * focal);
  const double ty = viewport[1] / (2 * focal);
  return {.DepthScale = std::hypot(1.0, tx, ty),
          .DisplacementScalePx = focal * std::hypot(std::numbers::sqrt2, tx + ty)};
}

double Distance(const Box &bounds, const Vec3 &eye) {
  Vec3 delta;
  for (size_t axis = 0; axis < 3; ++axis) {
    delta[axis] = std::max({bounds.Min[axis] - eye[axis], 0.0, eye[axis] - bounds.Max[axis]});
  }
  return std::hypot(delta[0], delta[1], delta[2]);
}

bool ValidPanoramicProjection(const Vec2 &viewport, ProjectedErrorBudget projection) {
  return std::isfinite(viewport[0]) && viewport[0] > 0 && std::isfinite(viewport[1]) &&
         viewport[1] > 0 && std::isfinite(projection.FocalPx) && projection.FocalPx > 0 &&
         std::isfinite(projection.AllowedErrorPx) && projection.AllowedErrorPx >= 0;
}

}

double SourceProductSelection::GeometryErrorLimitM(double distanceLowerM,
                                                   const Vec2 &viewportPx,
                                                   ProjectedErrorBudget projection) {
  if (!ValidPanoramicProjection(viewportPx, projection)) { return 0; }
  return Envelope(viewportPx, projection.FocalPx).Limit(distanceLowerM, projection.AllowedErrorPx);
}

double SourceProductSelection::GeometryErrorLimitM(const Box &bounds,
                                                   const Vec3 &eye,
                                                   const Vec2 &viewportPx,
                                                   ProjectedErrorBudget projection,
                                                   double translationM) {
  const auto finite = [](double value) { return std::isfinite(value); };
  if (!ValidPanoramicProjection(viewportPx, projection) || !bounds.Holds(bounds.Max) ||
      !std::ranges::all_of(bounds.Min, finite) || !std::ranges::all_of(bounds.Max, finite) ||
      !std::ranges::all_of(eye, finite) || !finite(translationM) || translationM < 0) {
    return 0;
  }
  return Envelope(viewportPx, projection.FocalPx)
      .Limit(std::max(0.0, Distance(bounds, eye) - translationM), projection.AllowedErrorPx);
}

double SourceProductSelection::GeometryErrorLimitM(const Box &bounds,
                                                   const ViewPath &path,
                                                   const Vec2 &viewportPx,
                                                   ProjectedErrorBudget projection) {
  if (!ValidPanoramicProjection(viewportPx, projection)) { return 0; }
  return Envelope(viewportPx, projection.FocalPx)
      .Limit(path.DistanceBelow(bounds), projection.AllowedErrorPx);
}

double SourceProductSelection::ProjectedSupportLimitM(const Box &bounds,
                                                      const ViewPath &path,
                                                      const Vec2 &viewportPx,
                                                      double focalPx,
                                                      double supportPx) {
  if (!ValidPanoramicProjection(viewportPx, {.FocalPx = focalPx, .AllowedErrorPx = supportPx})) {
    return 0;
  }
  const double distance = path.DistanceBelow(bounds);
  if (distance <= 0 || !std::isfinite(distance)) { return 0; }
  const auto upper = [](double value) {
    return std::nextafter(value, std::numeric_limits<double>::infinity());
  };
  const double tx = upper(upper(viewportPx[0] * 0.5) / focalPx);
  const double ty = upper(upper(viewportPx[1] * 0.5) / focalPx);
  const double denominator = upper(focalPx * upper(upper(1.0 + upper(tx * tx)) + upper(ty * ty)));
  const double limit = std::nextafter(std::nextafter(supportPx / denominator, 0.0) * distance, 0.0);
  return std::isfinite(limit) ? limit : 0.0;
}

std::expected<SourceProductSelection::Cut, SourceProductSelectionError>
SourceProductSelection::SelectGeometry(const Vec3 &eye,
                                       const Vec2 &viewportPx,
                                       ProjectedErrorBudget projection,
                                       double translationM,
                                       bool cohortGroups) const {
  const auto finite = [](double x) { return std::isfinite(x); };
  if (!Hierarchy_ || !std::ranges::all_of(eye, finite) ||
      !ValidPanoramicProjection(viewportPx, projection) || !finite(translationM) ||
      translationM < 0) {
    return std::unexpected(SourceProductSelectionError::InvalidInput);
  }
  const PanoramicError envelope = Envelope(viewportPx, projection.FocalPx);
  Cut cut{.Revision = Revision_, .Products = {}, .VisitedNodes = 0};
  std::optional<Product> selected;
  const auto nodes = Hierarchy_->Nodes();
  const auto candidate = [&](const PlanHierarchy::Node &node) {
    ++cut.VisitedNodes;
    const auto at = static_cast<uint32_t>(&node - nodes.data());
    const auto &traits = Nodes_[at];
    selected.reset();
    if (!traits.GeometrySupported || (cohortGroups && !traits.Cohort)) { return false; }
    const double distance = std::max(0.0, Distance(node.Bounds, eye) - translationM);
    const double hull = projection.AllowedErrorPx > 0
                            ? envelope.Bound(distance, {.WorldM = traits.Radius * 2})
                            : std::numeric_limits<double>::infinity();
    const double shell = traits.ShellErrorM
                             ? envelope.Bound(distance,
                                              {.WorldM = *traits.ShellErrorM,
                                               .BoundsExpansionM = *traits.ShellErrorM})
                             : std::numeric_limits<double>::infinity();
    selected = GeometryCandidate(at, hull, shell, projection.AllowedErrorPx);
    return selected.has_value();
  };
  Hierarchy_->SelectNodes(candidate, [&](const PlanHierarchy::Node &node) {
    const auto at = static_cast<uint32_t>(&node - nodes.data());
    if (node.Count == 1 && !candidate(node)) {
      selected = Product{.Node = at,
                         .Representation =
                             Nodes_[at].GeometrySupported ? Kind::Geometry : Kind::Unresolved};
    }
    cut.Products.push_back(*selected);
  });
  return cut;
}
}
