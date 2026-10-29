#include "TransportPlanFormat.h"
#include "TransportNetworkSnapshot.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <format>
#include <numbers>
#include <span>
#include <string_view>
#include <tuple>

namespace outshine::Generators::TransportFormat {
namespace {
constexpr double kLongitudeMostDeg = 180;
constexpr double kLatitudeMostDeg = 90;

bool Matches(const World::TransportEdge &edge, const TransportSourceMember &member) {
  return (member.End == 0 ? edge.FromNodeId : edge.ToNodeId) == member.Node &&
         edge.Layer == member.Layer && edge.Bridge == member.Bridge && edge.Tunnel == member.Tunnel;
}

bool TopologyBound(const World::TransportTopology &graph,
                   const TransportSourceEndTopology &topology) {
  const auto &ground = topology.Ground;
  const auto *edge = graph.FindEdge(ground.Edge);
  if (edge == nullptr || !Matches(*edge, ground)) { return false; }
  return std::ranges::all_of(topology.Relations, [&](const auto &relation) {
    const auto *related = graph.FindEdge(relation.Edge);
    if (topology.Kind == TransportSourceEndKind::External) { return related == nullptr; }
    return topology.Kind != TransportSourceEndKind::SourceNode ||
           (relation.Node == ground.Node && related != nullptr && Matches(*related, relation) &&
            relation.Layer == ground.Layer && relation.Bridge == ground.Bridge &&
            relation.Tunnel == ground.Tunnel);
  });
}

bool MemberValid(const TransportSourceMember &member) {
  if (member.Edge.PathId == 0 || member.Node == 0 || member.End > 1 ||
      member.Edge.Direction > World::EdgeDirection::Reverse || member.PointCount < 2 ||
      member.SourceStep >= member.PointCount - 1) {
    return false;
  }
  const auto physical =
      static_cast<uint8_t>(member.End ^ static_cast<uint8_t>(member.Edge.Direction));
  return !member.Terminal ||
         (member.OwnsEnd &&
          (physical == 0 ? member.SourceStep == 0 : member.SourceStep == member.PointCount - 2));
}
}

bool ValidSourceEnd(const TransportSourceEnd &end) {
  const auto &ground = end.Topology.Ground;
  const auto kind = end.Topology.Kind;
  const auto physical =
      static_cast<uint8_t>(ground.End ^ static_cast<uint8_t>(ground.Edge.Direction));
  if (kind > TransportSourceEndKind::Unresolved || !MemberValid(ground) ||
      !InputsValid(std::span(&end.Input, 1)) || end.Input.Version != "ground-source-end/1" ||
      std::ranges::all_of(end.ScopeBinding, [](uint8_t value) { return value == 0; }) ||
      end.Input.Content != TransportSourceEndContentBinding(end) ||
      std::ranges::all_of(end.Inputs, [](uint8_t value) { return value == 0; }) ||
      end.SourceId != std::format("native/{}/{}", ground.Edge.PathId, ground.Edge.SegmentOrdinal) ||
      end.Input.ItemId !=
          std::format(
              "native/{}/{}/{}", ground.Edge.PathId, ground.Edge.SegmentOrdinal, physical) ||
      end.Layer != ground.Layer || !ValidGate(end.Contact.Gate) || !ValidProfile(end.Profile) ||
      !ValidUse(end.Use) || !std::isfinite(end.Anchor.LongitudeDeg) ||
      !std::isfinite(end.Anchor.LatitudeDeg) ||
      std::abs(end.Anchor.LongitudeDeg) > kLongitudeMostDeg ||
      std::abs(end.Anchor.LatitudeDeg) > kLatitudeMostDeg ||
      !std::isfinite(end.Contact.Plane.SlopeE) || !std::isfinite(end.Contact.Plane.SlopeN) ||
      !std::isfinite(end.CrossfallRad) || std::abs(end.CrossfallRad) >= std::numbers::pi / 2) {
    return false;
  }
  if (kind == TransportSourceEndKind::Natural &&
      (!ground.Terminal || !ground.OwnsEnd || !end.Topology.Relations.empty())) {
    return false;
  }
  if ((kind == TransportSourceEndKind::SourceNode && end.Topology.Relations.size() != 1) ||
      (kind == TransportSourceEndKind::External &&
       (!ground.Terminal || end.Topology.Relations.empty()))) {
    return false;
  }
  for (size_t at = 0; at < end.Topology.Relations.size(); ++at) {
    const auto &relation = end.Topology.Relations[at];
    if (!MemberValid(relation) ||
        TransportSourceMemberKey(relation) == TransportSourceMemberKey(ground)) {
      return false;
    }
    if (kind == TransportSourceEndKind::External &&
        (!relation.Terminal || (!relation.Bridge && relation.Layer == 0) ||
         (relation.Tunnel && relation.Layer == 0))) {
      return false;
    }
    if (at != 0 && TransportSourceMemberKey(end.Topology.Relations[at - 1]) >=
                       TransportSourceMemberKey(relation)) {
      return false;
    }
  }
  return true;
}

bool SourceEndsBound(const TransportPlan &plan) {
  const auto key = [](const TransportInputReference &input) {
    return std::tuple{std::string_view(input.DatasetId),
                      std::string_view(input.Version),
                      std::string_view(input.Revision),
                      std::string_view(input.ItemId)};
  };
  return std::ranges::all_of(plan.SourceEnds, [&](const auto &end) {
    if (!ValidSourceEnd(end)) { return false; }
    const auto found = std::ranges::lower_bound(plan.Inputs, key(end.Input), {}, key);
    if (found == plan.Inputs.end() || key(*found) != key(end.Input) ||
        found->Content != end.Input.Content || !plan.Network) {
      return false;
    }
    const auto &identity = plan.Network->SourceIdentity();
    if (end.Input.DatasetId != identity.DatasetId || end.Input.Revision != identity.Revision) {
      return false;
    }
    const auto profileKey = std::tuple{std::string_view("outshine/ground-port-path"),
                                       std::string_view("1"),
                                       std::string_view("1"),
                                       std::string_view(end.SourceId)};
    const auto profile = std::ranges::lower_bound(plan.Inputs, profileKey, {}, key);
    if (profile == plan.Inputs.end() || key(*profile) != profileKey ||
        profile->Content != end.Inputs) {
      return false;
    }
    if (!TopologyBound(plan.Network->Topology(), end.Topology)) { return false; }
    if (plan.Coverage == TransportCoverage::Complete &&
        (end.Topology.Kind == TransportSourceEndKind::External ||
         end.Topology.Kind == TransportSourceEndKind::Unresolved)) {
      return false;
    }
    return true;
  });
}
}
