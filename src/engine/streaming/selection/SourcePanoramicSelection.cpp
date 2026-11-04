#include "SourceProductSelection.h"
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
};

double Distance(const Box &bounds, const Vec3 &eye) {
  Vec3 delta;
  for (size_t axis = 0; axis < 3; ++axis) {
    delta[axis] = std::max({bounds.Min[axis] - eye[axis], 0.0, eye[axis] - bounds.Max[axis]});
  }
  return std::hypot(delta[0], delta[1], delta[2]);
}
}

std::expected<SourceProductSelection::Cut, SourceProductSelectionError>
SourceProductSelection::SelectGeometry(const Vec3 &eye,
                                       const Vec2 &viewportPx,
                                       ProjectedErrorBudget projection,
                                       double translationM,
                                       bool cohortGroups) const {
  const auto finite = [](double x) { return std::isfinite(x); };
  if (!Hierarchy_ || !std::ranges::all_of(eye, finite) ||
      !std::ranges::all_of(viewportPx, finite) || viewportPx[0] <= 0 || viewportPx[1] <= 0 ||
      !finite(projection.FocalPx) || projection.FocalPx <= 0 ||
      !finite(projection.AllowedErrorPx) || projection.AllowedErrorPx < 0 ||
      !finite(translationM) || translationM < 0) {
    return std::unexpected(SourceProductSelectionError::InvalidInput);
  }
  const double tx = viewportPx[0] / (2 * projection.FocalPx);
  const double ty = viewportPx[1] / (2 * projection.FocalPx);
  const PanoramicError envelope{.DepthScale = std::hypot(1.0, tx, ty),
                                .DisplacementScalePx =
                                    projection.FocalPx * std::hypot(std::numbers::sqrt2, tx + ty)};
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
