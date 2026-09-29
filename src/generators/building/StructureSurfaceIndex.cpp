#include "StructureSurfaceIndex.h"
#include "DistanceInterval.h"
#include "Digest.h"
#include <cstddef>
#include <cstdint>
#include <optional>
#include <algorithm>
#include <bit>
#include <cassert>
#include <array>
#include <cmath>
#include <limits>

namespace outshine::Generators {

StructureSurfaceIndex::Match
StructureSurfaceIndex::MatchOf(const std::array<PointEnclosure, 3> &vertices) const noexcept {
  std::array<std::array<double, 3>, 3> points;
  for (size_t corner = 0; corner < points.size(); ++corner) {
    for (size_t axis = 0; axis < 3; ++axis) {
      points[corner][axis] = vertices[corner].EstimateM[axis];
    }
  }
  std::ranges::sort(points);
  uint64_t key = kDigestBasis;
  for (const auto &point : points) {
    for (const double coordinate : point) {
      const auto bits = std::bit_cast<uint64_t>(coordinate == 0 ? 0.0 : coordinate);
      key ^= bits;
      key *= kDigestPrime;
      key ^= key >> 32;
    }
  }
  return {.Key = key, .Slot = static_cast<size_t>(key) & (MatchSlots_ - 1)};
}

std::optional<size_t> StructureSurfaceIndex::NextMatch(Match &match) const noexcept {
  if (match.Remaining == 0) { return std::nullopt; }
  const Entry &entry = Matches_[match.Slot];
  match.Slot = (match.Slot + 1) & (MatchSlots_ - 1);
  --match.Remaining;
  if (entry.Triangle == std::numeric_limits<uint32_t>::max()) {
    match.Remaining = 0;
    return std::nullopt;
  }
  return entry.Key == match.Key ? std::optional<size_t>(entry.Triangle) : std::nullopt;
}

void StructureSurfaceIndex::Reset(const Raised &validated) {
  Input_ = &validated;
  const size_t count = (validated.WallRun.size() + validated.RoofRun.size()) / 3;
  assert(count > 0 && count <= 65536);
  MatchSlots_ = std::bit_ceil(count * 2);
  Matches_.clear();
  Matches_.reserve(MatchSlots_);
  Inserting_.reset();
  Nodes_.clear();
  Nodes_.reserve(count * 2 - 1);
  Nodes_.push_back({.Count = static_cast<uint32_t>(count)});
  Cursor_ = 0;
  Folding_ = count * 2 - 1;
}

std::array<PointEnclosure, 3> StructureSurfaceIndex::Triangle(size_t triangle) const noexcept {
  const size_t cursor = triangle * 3;
  const bool wall = cursor < Input_->WallRun.size();
  const auto &indices = wall ? Input_->WallRun : Input_->RoofRun;
  const auto &vertices = wall ? Input_->WallCorners : Input_->RoofCorners;
  const size_t offset = wall ? cursor : cursor - Input_->WallRun.size();
  std::array<PointEnclosure, 3> points;
  for (size_t corner = 0; corner < points.size(); ++corner) {
    const auto &point = vertices[indices[offset + corner]].pos;
    points[corner].EstimateM = {{point[0], point[1], point[2]}};
  }
  return points;
}

bool StructureSurfaceIndex::Step() {
  if (Matches_.size() < MatchSlots_) {
    Matches_.push_back({});
    return false;
  }
  if (Inserting_) {
    Entry &entry = Matches_[Insertion_.Slot];
    if (entry.Triangle == std::numeric_limits<uint32_t>::max()) {
      entry = {.Key = Insertion_.Key, .Triangle = *Inserting_};
      Inserting_.reset();
    } else if (--Insertion_.Remaining == 0) {
      Inserting_.reset();
    } else {
      Insertion_.Slot = (Insertion_.Slot + 1) & (MatchSlots_ - 1);
    }
    return false;
  }
  if (Cursor_ < Nodes_.size()) {
    Node &node = Nodes_[Cursor_++];
    if (node.Count == 1) {
      const auto triangle = Triangle(node.First);
      Inserting_ = node.First;
      Insertion_ = MatchOf(triangle);
      for (size_t axis = 0; axis < 3; ++axis) {
        node.MinM[axis] = static_cast<float>(std::min({triangle[0].EstimateM[axis],
                                                       triangle[1].EstimateM[axis],
                                                       triangle[2].EstimateM[axis]}));
        node.MaxM[axis] = static_cast<float>(std::max({triangle[0].EstimateM[axis],
                                                       triangle[1].EstimateM[axis],
                                                       triangle[2].EstimateM[axis]}));
      }
    } else {
      const uint32_t count = node.Count;
      const uint32_t first = node.First;
      node.Left = static_cast<uint32_t>(Nodes_.size());
      node.Right = node.Left + 1;
      Nodes_.push_back({.First = first, .Count = count / 2});
      Nodes_.push_back({.First = first + count / 2, .Count = count - count / 2});
    }
    return false;
  }
  if (Folding_ > 0) {
    Node &node = Nodes_[--Folding_];
    if (node.Count > 1) {
      const Node &left = Nodes_[node.Left];
      const Node &right = Nodes_[node.Right];
      for (size_t axis = 0; axis < 3; ++axis) {
        node.MinM[axis] = std::min(left.MinM[axis], right.MinM[axis]);
        node.MaxM[axis] = std::max(left.MaxM[axis], right.MaxM[axis]);
      }
    }
  }
  return Folding_ == 0;
}

double StructureSurfaceIndex::LowerDistance(size_t node, Vec3 point) const noexcept {
  using namespace DistanceArithmetic;
  const Node &bounds = Nodes_[node];
  Interval squared{.Lower = 0, .Upper = 0};
  for (size_t axis = 0; axis < 3; ++axis) {
    const double value = point[axis];
    Interval gap{.Lower = 0, .Upper = 0};
    if (value < bounds.MinM[axis]) {
      gap = Subtract({.Lower = bounds.MinM[axis], .Upper = bounds.MinM[axis]},
                     {.Lower = value, .Upper = value});
    } else if (value > bounds.MaxM[axis]) {
      gap = Subtract({.Lower = value, .Upper = value},
                     {.Lower = bounds.MaxM[axis], .Upper = bounds.MaxM[axis]});
    }
    gap.Lower = std::max(0.0, gap.Lower);
    squared = Add(squared, Multiply(gap, gap));
  }
  return std::max(0.0,
                  std::nextafter(std::sqrt(std::max(0.0, squared.Lower)),
                                 -std::numeric_limits<double>::infinity()));
}

}
