#include "RiverBodyPlan.h"
#include "TileSourceIdentity.h"
#include "math/Box.h"
#include "math/Vec3.h"
#include "math/Units.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <map>
#include <numeric>
#include <optional>
#include <limits>
#include <span>
#include <stop_token>
#include <string_view>
#include <tuple>
#include <utility>
#include <vector>

namespace outshine::Generators {
namespace {
using Error = WaterTopologyError;
using Scope = std::tuple<Data::TileSourceIdentity::Origin,
                         Data::DataKind,
                         std::string_view,
                         std::string_view,
                         std::string_view>;

uint32_t Root(std::span<uint32_t> parents, uint32_t at) {
  while (parents[at] != at) {
    parents[at] = parents[parents[at]];
    at = parents[at];
  }
  return at;
}

bool Overlap(const Box &a, const Box &b) {
  return a.Min[0] <= b.Max[0] && b.Min[0] <= a.Max[0] && a.Min[1] <= b.Max[1] &&
         b.Min[1] <= a.Max[1];
}

std::optional<Scope> RiverScope(const WaterAsset &water, const WaterAsset::Surface &surface) {
  const auto *input = water.InputOf(surface);
  if (input == nullptr || input->Kind != "river" || input->Bridge || input->Tunnel ||
      !input->Crossing.empty() || input->Source >= water.Sources().size()) {
    return std::nullopt;
  }
  const auto &source = water.Sources()[input->Source].Identity;
  if (source.SourceId.empty() || source.Revision.empty()) { return std::nullopt; }
  return Scope{source.From, source.Kind, source.SourceId, source.Revision, input->Layer};
}

std::expected<Box, Error> SourceBounds(const WaterAsset &water,
                                       uint32_t surface,
                                       const TangentFrame &frame,
                                       uint32_t coordinatesMost) {
  Box bounds;
  uint64_t coordinates = 0;
  const auto points = water.Points();
  for (const auto &ring : water.RingsOf(water.Surfaces()[surface])) {
    coordinates += ring.PointCount;
    if (coordinates > coordinatesMost) { return std::unexpected(Error::CapacityExceeded); }
    if (ring.PointCount < 3 || ring.FirstPoint > points.size() / 2 ||
        ring.PointCount > points.size() / 2 - ring.FirstPoint) {
      return std::unexpected(Error::InvalidGeometry);
    }
    for (uint32_t at = 0; at < ring.PointCount; ++at) {
      const size_t point = (static_cast<size_t>(ring.FirstPoint) + at) * 2;
      const LongitudeLatitude geographic{.LongitudeDeg = points[point + 1],
                                         .LatitudeDeg = points[point]};
      if (!std::isfinite(geographic.LongitudeDeg) || !std::isfinite(geographic.LatitudeDeg) ||
          std::abs(geographic.LongitudeDeg) > kDegPerHalfTurn ||
          std::abs(geographic.LatitudeDeg) > kDegPerHalfTurn / 2) {
        return std::unexpected(Error::InvalidGeometry);
      }
      const auto xy = frame.ToLocalGroundPosition(geographic);
      if (!std::isfinite(xy.EastM) || !std::isfinite(xy.NorthM)) {
        return std::unexpected(Error::InvalidGeometry);
      }
      bounds.Cover(Vec3{{xy.EastM, xy.NorthM, 0}});
    }
  }
  if (bounds.Empty()) { return std::unexpected(Error::InvalidGeometry); }
  return bounds;
}

std::expected<std::vector<std::vector<uint32_t>>, Error>
EnvelopeComponents(std::span<const uint32_t> indices,
                   std::span<const Box> bounds,
                   WaterGroundPolicy policy,
                   uint64_t pairsMost,
                   uint64_t &pairs) {
  auto hierarchy = PlanHierarchy::Build(bounds);
  if (!hierarchy) { return std::unexpected(Error::InvalidGeometry); }
  std::vector<uint32_t> parents(indices.size());
  std::ranges::iota(parents, uint32_t{0});
  if (policy == WaterGroundPolicy::AssumeUnlabelledGround) {
    for (uint32_t at = 0; at < indices.size(); ++at) {
      hierarchy->SelectNodes(
          [&](const PlanHierarchy::Node &node) {
            return pairs > pairsMost || !Overlap(bounds[at], node.Bounds);
          },
          [&](const PlanHierarchy::Node &node) {
            if (node.Count != 1 || pairs > pairsMost || !Overlap(bounds[at], node.Bounds)) {
              return;
            }
            const auto other = hierarchy->SourceOrder()[node.First];
            if (other <= at) { return; }
            ++pairs;
            const auto a = Root(parents, at);
            const auto b = Root(parents, other);
            parents[std::max(a, b)] = std::min(a, b);
          });
      if (pairs > pairsMost) { return std::unexpected(Error::CapacityExceeded); }
    }
  }
  std::map<uint32_t, std::vector<uint32_t>> groups;
  for (uint32_t at = 0; at < indices.size(); ++at) {
    groups[Root(parents, at)].push_back(indices[at]);
  }
  std::vector<std::vector<uint32_t>> result;
  result.reserve(groups.size());
  for (auto &[root, sources] : groups) { result.push_back(std::move(sources)); }
  return result;
}

void Reject(RiverBodyPlan &plan, std::span<const uint32_t> indices, Error reason) {
  const auto rejected = reason == Error::CapacityExceeded
                            ? WaterAsset::SurfaceRejectionReason::CapacityExceeded
                            : WaterAsset::SurfaceRejectionReason::InvalidTopology;
  for (const auto index : indices) {
    plan.Rejections.push_back({.Surface = index, .Reason = rejected});
  }
}

std::expected<void, Error> BuildBucket(const WaterAsset &water,
                                       const TangentFrame &frame,
                                       std::span<const uint32_t> bucket,
                                       WaterGroundPolicy policy,
                                       const WaterTopologyBudget &budget,
                                       const std::stop_token &stop,
                                       RiverBodyPlan &plan) {
  std::vector<uint32_t> indices;
  std::vector<Box> bounds;
  for (const auto index : bucket) {
    const auto box = SourceBounds(water, index, frame, budget.SourceCoordinates);
    if (!box) {
      Reject(plan, std::span(&index, 1), box.error());
      continue;
    }
    indices.push_back(index);
    bounds.push_back(*box);
  }
  uint64_t pairs = 0;
  const auto groups = EnvelopeComponents(indices, bounds, policy, budget.Intersections, pairs);
  plan.EnvelopePairs += pairs;
  if (!groups) {
    Reject(plan, indices, groups.error());
    return {};
  }
  for (const auto &selected : *groups) {
    auto topology = BuildWaterSurfaceTopology(water, selected, frame, budget, stop);
    if (!topology) {
      if (topology.error() == Error::Canceled) { return std::unexpected(Error::Canceled); }
      Reject(plan, selected, topology.error());
      continue;
    }
    auto bodies = SplitWaterSurfaceComponents(std::move(*topology), stop);
    if (!bodies) {
      if (bodies.error() == Error::Canceled) { return std::unexpected(Error::Canceled); }
      Reject(plan, selected, bodies.error());
      continue;
    }
    for (auto &body : *bodies) {
      plan.Bodies.push_back({.Surface = std::move(body), .FaceIndex = {}, .Courses = {}});
    }
  }
  return {};
}
}

std::expected<RiverBodyPlan, WaterTopologyError> PlanRiverBodies(const WaterAsset &water,
                                                                 const TangentFrame &frame,
                                                                 WaterGroundPolicy policy,
                                                                 WaterTopologyBudget budget,
                                                                 const std::stop_token &stop) {
  if (stop.stop_requested()) { return std::unexpected(Error::Canceled); }
  const auto anchor = frame.Anchor();
  if (!std::isfinite(anchor.LongitudeDeg) || !std::isfinite(anchor.LatitudeDeg) ||
      std::abs(anchor.LongitudeDeg) > kDegPerHalfTurn ||
      std::abs(anchor.LatitudeDeg) > kDegPerHalfTurn / 2 ||
      water.Surfaces().size() > std::numeric_limits<uint32_t>::max()) {
    return std::unexpected(Error::InvalidInput);
  }
  std::map<Scope, std::vector<uint32_t>> buckets;
  for (uint32_t at = 0; at < water.Surfaces().size(); ++at) {
    const auto scope = RiverScope(water, water.Surfaces()[at]);
    if (scope) { buckets[*scope].push_back(at); }
  }
  RiverBodyPlan plan;
  for (const auto &[scope, bucket] : buckets) {
    const auto result = BuildBucket(water, frame, bucket, policy, budget, stop, plan);
    if (!result) { return std::unexpected(result.error()); }
  }
  const auto courses = BindRiverCourses(water, frame, plan, stop);
  if (!courses) { return std::unexpected(courses.error()); }
  return plan;
}
}
