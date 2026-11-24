#include "SourceProductSelection.h"
#include "DistanceInterval.h"
#include <algorithm>
#include <array>
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

Vec3 ClosestSeparatingAxis(const Box &bounds, const ViewPath &path) {
  const Vec3 advance = path.End - path.Start;
  const Vec3 minimum = bounds.Min - path.Start;
  const Vec3 maximum = bounds.Max - path.Start;
  const auto finite = [](double value) { return std::isfinite(value); };
  if (!std::ranges::all_of(advance, finite) || !std::ranges::all_of(minimum, finite) ||
      !std::ranges::all_of(maximum, finite)) {
    return {};
  }
  std::array<double, 8> parameters{0, 1};
  size_t count = 2;
  for (size_t axis = 0; axis < 3; ++axis) {
    if (advance[axis] == 0) { continue; }
    parameters[count++] = std::clamp(minimum[axis] / advance[axis], 0.0, 1.0);
    parameters[count++] = std::clamp(maximum[axis] / advance[axis], 0.0, 1.0);
  }
  std::sort(parameters.begin(), parameters.begin() + static_cast<std::ptrdiff_t>(count));
  Vec3 best;
  double squared = std::numeric_limits<double>::infinity();
  for (size_t at = 1; at < count; ++at) {
    const double first = parameters[at - 1];
    const double last = parameters[at];
    const Vec3 middle = advance * ((first + last) * 0.5);
    double sum = 0;
    double weight = 0;
    for (size_t axis = 0; axis < 3; ++axis) {
      const double side = std::clamp(middle[axis], minimum[axis], maximum[axis]);
      if (side == middle[axis]) { continue; }
      sum += side * advance[axis];
      weight += advance[axis] * advance[axis];
    }
    const Vec3 eye = advance * (weight > 0 ? std::clamp(sum / weight, first, last) : first);
    const Vec3 closest{{std::clamp(eye[0], minimum[0], maximum[0]),
                        std::clamp(eye[1], minimum[1], maximum[1]),
                        std::clamp(eye[2], minimum[2], maximum[2])}};
    const Vec3 separation = eye - closest;
    const double candidate = Dot(separation, separation);
    if (candidate < squared) {
      best = separation;
      squared = candidate;
    }
  }
  return best;
}

double PathDistanceBelow(const Box &bounds, const ViewPath &path) {
  using namespace DistanceArithmetic;
  Vec3 normal = ClosestSeparatingAxis(bounds, path);
  if (!Normalise(normal)) { return 0; }
  std::array<Interval, 3> components{};
  Interval start{};
  Interval finish{};
  for (size_t axis = 0; axis < 3; ++axis) {
    const double support = normal[axis] < 0 ? bounds.Min[axis] : bounds.Max[axis];
    const Interval plane{support, support};
    components[axis] = {normal[axis], normal[axis]};
    start = Add(start,
                Multiply(components[axis], Subtract({path.Start[axis], path.Start[axis]}, plane)));
    finish =
        Add(finish, Multiply(components[axis], Subtract({path.End[axis], path.End[axis]}, plane)));
  }
  const double separation = std::min(start.Lower, finish.Lower);
  return separation > 0 ? std::nextafter(separation / UpperNorm(components), 0.0) : 0;
}

bool ValidPanoramicProjection(const Vec2 &viewport, ProjectedErrorBudget projection) {
  return std::isfinite(viewport[0]) && viewport[0] > 0 && std::isfinite(viewport[1]) &&
         viewport[1] > 0 && std::isfinite(projection.FocalPx) && projection.FocalPx > 0 &&
         std::isfinite(projection.AllowedErrorPx) && projection.AllowedErrorPx >= 0;
}
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
  const auto finite = [](double value) { return std::isfinite(value); };
  if (!ValidPanoramicProjection(viewportPx, projection) ||
      !std::ranges::all_of(path.Start, finite) || !std::ranges::all_of(path.End, finite) ||
      !finite(path.RadiusM) || path.RadiusM < 0 || !std::ranges::all_of(bounds.Min, finite) ||
      !std::ranges::all_of(bounds.Max, finite) || !bounds.Holds(bounds.Max)) {
    return 0;
  }
  const double distance = PathDistanceBelow(bounds, path);
  return Envelope(viewportPx, projection.FocalPx)
      .Limit(std::nextafter(distance - path.RadiusM, 0.0), projection.AllowedErrorPx);
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
