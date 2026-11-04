#include "SourceProductSelection.h"
#include <algorithm>
#include <bit>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <utility>

namespace outshine {
std::expected<SourceProductSelection, SourceProductSelectionError>
SourceProductSelection::Of(std::shared_ptr<const PlanHierarchy> hierarchy,
                           std::span<const Source> sources,
                           std::string revision) {
  if (!hierarchy || sources.empty() || hierarchy->SourceOrder().size() != sources.size() ||
      revision.empty()) {
    return std::unexpected(SourceProductSelectionError::InvalidInput);
  }
  SourceProductSelection result;
  result.Hierarchy_ = std::move(hierarchy);
  result.Revision_ = std::move(revision);
  const auto nodes = result.Hierarchy_->Nodes();
  result.Nodes_.resize(nodes.size());
  for (size_t at = nodes.size(); at-- > 0;) {
    const auto &source = nodes[at];
    auto &node = result.Nodes_[at];
    node.Centre = source.Bounds.Middle();
    node.Radius = Length(source.Bounds.Span()) / 2;
    if (!std::isfinite(node.Radius)) {
      return std::unexpected(SourceProductSelectionError::InvalidInput);
    }
    if (source.Count == 1) {
      const auto &leaf = sources[result.Hierarchy_->SourceOrder()[source.First]];
      node.Cohort = leaf.Cohort;
      node.Supported = leaf.FieldSupported;
    } else {
      const auto &left = result.Nodes_[source.Left];
      const auto &right = result.Nodes_[source.Right];
      node.Cohort = left.Cohort == right.Cohort ? left.Cohort : std::nullopt;
      node.Supported = left.Supported && right.Supported;
    }
  }
  return result;
}

std::optional<SourceProductSelection::Product> SourceProductSelection::Candidate(
    uint32_t at, const Vec3 &eye, ProjectedErrorBudget projection) const {
  const auto &node = Nodes_[at];
  if (!node.Supported || node.Radius <= 0 || projection.AllowedErrorPx <= 0) {
    return std::nullopt;
  }
  const double distance = Length(node.Centre - eye);
  if (distance <= node.Radius) { return std::nullopt; }
  const double required =
      std::ceil(4 * projection.FocalPx * node.Radius / (distance - node.Radius) + 2);
  if (!std::isfinite(required) || required > 4096) { return std::nullopt; }
  const uint32_t pixels = std::bit_ceil(std::max(uint32_t{4}, static_cast<uint32_t>(required)));
  const double half = node.Radius * pixels / (pixels - 2u);
  if (distance <= 2 * half) { return std::nullopt; }
  const double error = 4 * projection.FocalPx * half * half / (distance * (distance - 2 * half));
  if (!std::isfinite(error) || error > projection.AllowedErrorPx) { return std::nullopt; }
  return Product{
      .Node = at, .Representation = Kind::MaterialField, .Pixels = pixels, .CentredErrorPx = error};
}

std::expected<SourceProductSelection::Cut, SourceProductSelectionError>
SourceProductSelection::Select(const Vec3 &eye,
                               ProjectedErrorBudget projection,
                               bool cohortGroups) const {
  if (!Hierarchy_ || !std::ranges::all_of(eye, [](double value) { return std::isfinite(value); }) ||
      !std::isfinite(projection.FocalPx) || projection.FocalPx <= 0 ||
      !std::isfinite(projection.AllowedErrorPx) || projection.AllowedErrorPx < 0) {
    return std::unexpected(SourceProductSelectionError::InvalidInput);
  }
  Cut cut{.Revision = Revision_, .Products = {}, .VisitedNodes = 0};
  std::optional<Product> selected;
  const auto nodes = Hierarchy_->Nodes();
  Hierarchy_->SelectNodes(
      [&](const PlanHierarchy::Node &node) {
        ++cut.VisitedNodes;
        const auto at = static_cast<uint32_t>(&node - nodes.data());
        selected =
            !cohortGroups || Nodes_[at].Cohort ? Candidate(at, eye, projection) : std::nullopt;
        return selected.has_value();
      },
      [&](const PlanHierarchy::Node &node) {
        const auto at = static_cast<uint32_t>(&node - nodes.data());
        if (node.Count == 1) {
          ++cut.VisitedNodes;
          selected = Candidate(at, eye, projection);
          if (!selected) {
            selected =
                Product{.Node = at,
                        .Representation = Nodes_[at].Supported ? Kind::Geometry : Kind::Unresolved};
          }
        }
        cut.Products.push_back(*selected);
      });
  return cut;
}

std::expected<std::span<const uint32_t>, SourceProductSelectionError>
SourceProductSelection::Members(const Cut &cut, const Product &product) const {
  if (cut.Revision != Revision_) {
    return std::unexpected(SourceProductSelectionError::ForeignRevision);
  }
  if (product.Node >= Nodes_.size()) {
    return std::unexpected(SourceProductSelectionError::InvalidProduct);
  }
  return Hierarchy_->Members(Hierarchy_->Nodes()[product.Node]);
}

const Box &SourceProductSelection::Bounds(const Product &product) const {
  return Hierarchy_->Nodes()[product.Node].Bounds;
}
}
