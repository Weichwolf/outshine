#include "PlantPlan.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <span>
#include <string_view>
#include <vector>
#include <cmath>
#include <limits>
#include <numbers>
#include <set>
#include <unordered_set>
#include <utility>

namespace outshine::Generators {
namespace {
constexpr double kFrameTolerance = 1e-10;

template <size_t N> bool Finite(const std::array<double, N> &values) {
  return std::ranges::all_of(values, [](double value) { return std::isfinite(value); });
}

bool FrameValid(const std::array<double, 9> &frame) {
  if (!Finite(frame)) { return false; }
  for (size_t a = 0; a < 3; ++a) {
    for (size_t b = a; b < 3; ++b) {
      double dot = 0;
      for (size_t row = 0; row < 3; ++row) { dot += frame[row * 3 + a] * frame[row * 3 + b]; }
      if (std::abs(dot - (a == b ? 1.0 : 0.0)) > kFrameTolerance) { return false; }
    }
  }
  const double determinant = frame[0] * (frame[4] * frame[8] - frame[5] * frame[7]) -
                             frame[1] * (frame[3] * frame[8] - frame[5] * frame[6]) +
                             frame[2] * (frame[3] * frame[7] - frame[4] * frame[6]);
  return std::abs(determinant - 1.0) <= kFrameTolerance;
}

size_t Depth(const PlantAxis &axis) {
  return static_cast<size_t>(std::ranges::find(axis.Path, int16_t{-1}) - axis.Path.begin());
}

bool AxisValid(const PlantAxis &axis) {
  const size_t depth = Depth(axis);
  return std::ranges::all_of(std::span(axis.Path).first(depth),
                             [](int16_t slot) { return slot >= 0; }) &&
         std::ranges::all_of(std::span(axis.Path).subspan(depth),
                             [](int16_t slot) { return slot == -1; }) &&
         Finite(axis.OriginM) && FrameValid(axis.Frame) && std::isfinite(axis.LengthM) &&
         axis.LengthM > 0 && std::isfinite(axis.Bend) && std::abs(axis.Bend) <= std::numbers::pi &&
         std::isfinite(axis.ParentParameter) && axis.ParentParameter >= 0 &&
         axis.ParentParameter <= 1 && Finite(axis.RadiiM) && axis.RadiiM[0] > 0 &&
         axis.RadiiM[1] > 0;
}
}

bool PlantPlan::IndexAxes() {
  std::set<std::array<int16_t, 7>> paths;
  ById_.reserve(Axes_.size());
  Index_.resize(Axes_.size());
  for (size_t at = 0; at < Axes_.size(); ++at) {
    const auto &axis = Axes_[at];
    if (!AxisValid(axis) || !ById_.emplace(axis.Id, at).second || !paths.insert(axis.Path).second) {
      return false;
    }
    if (at == 0) {
      if (axis.Parent != -1 || Depth(axis) != 0 || axis.ParentParameter != 0) { return false; }
      continue;
    }
    if (axis.Parent < 0) { return false; }
    const auto parent = ById_.find(static_cast<uint64_t>(axis.Parent));
    if (parent == ById_.end() || parent->second >= at) { return false; }
    const auto &ancestor = Axes_[parent->second];
    const size_t depth = Depth(ancestor);
    if (Depth(axis) != depth + 1 || !std::equal(axis.Path.begin(),
                                                axis.Path.begin() + static_cast<ptrdiff_t>(depth),
                                                ancestor.Path.begin())) {
      return false;
    }
    Index_[at].Parent = parent->second;
  }
  return true;
}

bool PlantPlan::IndexLeaves(size_t variants) {
  std::unordered_set<uint64_t> identities;
  identities.reserve(Leaves_.size());
  size_t previous = 0;
  for (size_t at = 0; at < Leaves_.size(); ++at) {
    const auto &leaf = Leaves_[at];
    const auto owner = ById_.find(leaf.Axis);
    if (owner == ById_.end() || owner->second < previous || leaf.Variant >= variants ||
        !Finite(leaf.OriginM) || !Finite(leaf.NodeM) || !FrameValid(leaf.Frame) ||
        !std::isfinite(leaf.StemParameter) || leaf.StemParameter < 0 || leaf.StemParameter > 1 ||
        !identities.insert(leaf.Id).second) {
      return false;
    }
    previous = owner->second;
    auto &index = Index_[previous];
    if (index.LeafCount == 0) { index.FirstLeaf = at; }
    ++index.LeafCount;
    ++index.Leaves;
    ++Variants_[previous * variants + leaf.Variant];
  }
  for (size_t at = Axes_.size(); at-- > 1;) {
    auto &parent = Index_[Index_[at].Parent];
    parent.Axes += Index_[at].Axes;
    parent.Leaves += Index_[at].Leaves;
    for (size_t variant = 0; variant < variants; ++variant) {
      Variants_[Index_[at].Parent * variants + variant] += Variants_[at * variants + variant];
    }
  }
  return true;
}

std::expected<PlantPlan, std::string_view>
PlantPlan::Create(std::vector<PlantAxis> axes, std::vector<PlantLeaf> leaves, size_t variants) {
  if (axes.empty() || variants == 0 || variants > size_t{1} + std::numeric_limits<uint8_t>::max() ||
      axes.size() > std::vector<size_t>{}.max_size() / variants) {
    return std::unexpected("plant plan requires bounded axes and blade variants");
  }
  PlantPlan result;
  result.Axes_ = std::move(axes);
  result.Leaves_ = std::move(leaves);
  result.VariantCount_ = variants;
  result.Variants_.resize(result.Axes_.size() * variants);
  if (!result.IndexAxes() || !result.IndexLeaves(variants)) {
    return std::unexpected("plant plan requires canonical rooted axes and owned finite leaf rows");
  }
  return result;
}

const PlantAxis *PlantPlan::Axis(uint64_t id) const noexcept {
  const auto found = ById_.find(id);
  return found == ById_.end() ? nullptr : &Axes_[found->second];
}

std::expected<PlantPlan::Summary, std::string_view>
PlantPlan::Summarize(uint64_t id) const noexcept {
  const auto found = ById_.find(id);
  if (found == ById_.end()) { return std::unexpected("plant axis is absent"); }
  const auto &index = Index_[found->second];
  return Summary{.Axes = index.Axes,
                 .Leaves = index.Leaves,
                 .Variants =
                     std::span(Variants_).subspan(found->second * VariantCount_, VariantCount_)};
}

std::expected<std::span<const PlantLeaf>, std::string_view>
PlantPlan::Leaves(LeafDemand demand) const noexcept {
  const auto found = ById_.find(demand.Axis);
  if (found == ById_.end()) { return std::unexpected("plant axis is absent"); }
  const auto &index = Index_[found->second];
  if (index.LeafCount > demand.Maximum) {
    return std::unexpected("plant leaf detail exceeds demand budget");
  }
  return std::span(Leaves_).subspan(index.FirstLeaf, index.LeafCount);
}
}
