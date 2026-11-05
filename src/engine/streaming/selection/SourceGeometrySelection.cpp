#include "SourceProductSelection.h"
#include "Viewing.h"
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
struct ProjectionDomain {
  double Width = 0, Height = 0, Depth = 0;
  double XMost = 0, YMost = 0, ZLeast = 0;
};

bool ValidCamera(const Render::CameraBasis &camera) {
  constexpr double basisTolerance = 1e-12;
  const std::array axes{camera.Right, camera.Up, camera.Forward};
  const auto finite = [](double value) { return std::isfinite(value); };
  if (!std::ranges::all_of(camera.EyeM, finite)) { return false; }
  for (size_t at = 0; at < axes.size(); ++at) {
    if (!std::ranges::all_of(axes[at], finite)) { return false; }
    for (size_t other = 0; other <= at; ++other) {
      if (std::abs(Dot(axes[at], axes[other]) - (at == other ? 1.0 : 0.0)) > basisTolerance) {
        return false;
      }
    }
  }
  return true;
}

ProjectionDomain Domain(const Box &box, const Render::CameraBasis &camera, double translationM) {
  const auto centre = box.Middle() - camera.EyeM;
  const auto half = box.Span() * 0.5;
  const std::array axes{camera.Right, camera.Up, camera.Forward};
  std::array<double, 3> middle{};
  std::array<double, 3> extent{};
  for (size_t at = 0; at < axes.size(); ++at) {
    middle[at] = Dot(centre, axes[at]);
    for (size_t dim = 0; dim < 3; ++dim) { extent[at] += std::abs(axes[at][dim]) * half[dim]; }
  }
  return {.Width = 2 * extent[0],
          .Height = 2 * extent[1],
          .Depth = 2 * extent[2],
          .XMost = std::abs(middle[0]) + extent[0] + translationM,
          .YMost = std::abs(middle[1]) + extent[1] + translationM,
          .ZLeast = middle[2] - extent[2] - translationM};
}

double HullError(const ProjectionDomain &domain, double focal, bool orthographic) {
  if (orthographic) { return focal * (domain.Width + domain.Height); }
  if (domain.ZLeast <= 0) { return std::numeric_limits<double>::infinity(); }
  const double slope = (domain.XMost + domain.YMost) / domain.ZLeast;
  return focal * (domain.Width + domain.Height + slope * domain.Depth) / domain.ZLeast;
}

double ShellError(const ProjectionDomain &domain, double focal, double error, bool orthographic) {
  if (error == 0) { return 0; }
  if (orthographic) { return std::numbers::sqrt2 * focal * error; }
  if (domain.ZLeast <= error) { return std::numeric_limits<double>::infinity(); }
  const double slope = (domain.XMost + domain.YMost) / domain.ZLeast;
  return focal * error * (std::numbers::sqrt2 + slope) / (domain.ZLeast - error);
}

double
ShellLimit(const ProjectionDomain &domain, ProjectedErrorBudget projection, bool orthographic) {
  const double slope = orthographic ? 0 : (domain.XMost + domain.YMost) / domain.ZLeast;
  const double infinity = std::numeric_limits<double>::infinity();
  const double factor = std::nextafter(std::numbers::sqrt2 + slope, infinity);
  const double product = std::nextafter(projection.FocalPx * factor, infinity);
  const double denominator =
      std::nextafter(product + (orthographic ? 0 : projection.AllowedErrorPx), infinity);
  if (!std::isfinite(denominator) || denominator <= 0 || (!orthographic && domain.ZLeast <= 0)) {
    return 0;
  }
  const double fraction = std::nextafter(projection.AllowedErrorPx / denominator, 0.0);
  const double limit = fraction * (orthographic ? 1 : domain.ZLeast);
  if (!std::isfinite(limit) || limit <= 0) { return 0; }
  const double guarded = std::nextafter(limit, 0.0);
  return ShellError(domain, projection.FocalPx, guarded, orthographic) <= projection.AllowedErrorPx
             ? guarded
             : 0;
}
}

std::optional<SourceProductSelection::Product> SourceProductSelection::GeometryCandidate(
    uint32_t at, double hullPx, double shellPx, double allowedPx) const {
  const auto &node = Nodes_[at];
  if (hullPx <= allowedPx && (Hierarchy_->Nodes()[at].Count > 1 || shellPx > allowedPx)) {
    return Product{.Node = at,
                   .Representation = Kind::Hull,
                   .ProjectedErrorPx = hullPx,
                   .WorldErrorM = 2 * node.Radius};
  }
  if (shellPx <= allowedPx && node.ShellErrorM) {
    return Product{.Node = at,
                   .Representation = Kind::Shell,
                   .ProjectedErrorPx = shellPx,
                   .WorldErrorM = *node.ShellErrorM};
  }
  return std::nullopt;
}

double SourceProductSelection::GeometryErrorPx(const Box &bounds,
                                               const Render::CameraBasis &camera,
                                               double focalPx,
                                               double errorM) {
  if (!ValidCamera(camera) || bounds.Empty() || !std::isfinite(focalPx) || focalPx <= 0 ||
      !std::isfinite(errorM) || errorM < 0) {
    return std::numeric_limits<double>::infinity();
  }
  return ShellError(Domain(bounds, camera, 0), focalPx, errorM, false);
}

double SourceProductSelection::GeometryErrorLimitM(const Box &bounds,
                                                   const Render::CameraBasis &camera,
                                                   ProjectedErrorBudget projection,
                                                   double translationM,
                                                   bool orthographic) {
  if (!ValidCamera(camera) || bounds.Empty() || !std::isfinite(translationM) || translationM < 0 ||
      !std::isfinite(projection.FocalPx) || projection.FocalPx <= 0 ||
      !std::isfinite(projection.AllowedErrorPx) || projection.AllowedErrorPx < 0) {
    return 0;
  }
  for (size_t axis = 0; axis < 3; ++axis) {
    if (!std::isfinite(bounds.Min[axis]) || !std::isfinite(bounds.Max[axis])) { return 0; }
  }
  return ShellLimit(Domain(bounds, camera, translationM), projection, orthographic);
}

std::expected<SourceProductSelection::Cut, SourceProductSelectionError>
SourceProductSelection::SelectGeometry(const Render::CameraBasis &camera,
                                       ProjectedErrorBudget projection,
                                       double translationM,
                                       bool orthographic,
                                       bool cohortGroups) const {
  if (!Hierarchy_ || !ValidCamera(camera) || !std::isfinite(translationM) || translationM < 0 ||
      !std::isfinite(projection.FocalPx) || projection.FocalPx <= 0 ||
      !std::isfinite(projection.AllowedErrorPx) || projection.AllowedErrorPx < 0) {
    return std::unexpected(SourceProductSelectionError::InvalidInput);
  }
  Cut cut{.Revision = Revision_, .Products = {}, .VisitedNodes = 0};
  std::optional<Product> selected;
  const auto nodes = Hierarchy_->Nodes();
  const auto candidate = [&](const PlanHierarchy::Node &node) {
    ++cut.VisitedNodes;
    const auto at = static_cast<uint32_t>(&node - nodes.data());
    const auto &traits = Nodes_[at];
    selected.reset();
    if (!traits.GeometrySupported || (cohortGroups && !traits.Cohort)) { return false; }
    const auto domain = Domain(node.Bounds, camera, translationM);
    const double hull = projection.AllowedErrorPx > 0
                            ? HullError(domain, projection.FocalPx, orthographic)
                            : std::numeric_limits<double>::infinity();
    const double shell =
        traits.ShellErrorM
            ? ShellError(domain, projection.FocalPx, *traits.ShellErrorM, orthographic)
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
