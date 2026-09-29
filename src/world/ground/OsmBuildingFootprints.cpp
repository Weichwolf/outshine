#include "OsmBuildingFootprints.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <memory>
#include <limits>
#include <span>
#include <vector>
#include <map>
#include <ranges>
#include <set>
#include <string_view>
#include <utility>

namespace outshine::Ground {

namespace {

std::string_view Tag(std::span<const Data::OsmTag> tags, std::string_view key) {
  for (const auto &tag : tags) {
    if (tag.Key == key) { return tag.Value; }
  }
  return {};
}

bool Ambiguous(std::span<const Data::OsmTag> tags) {
  for (const std::string_view key : {"building", "building:part", "man_made", "type"}) {
    if (std::ranges::count(tags, key, &Data::OsmTag::Key) > 1) { return true; }
  }
  return false;
}

bool IsBuilding(std::span<const Data::OsmTag> tags) {
  return std::ranges::any_of(tags, [](const Data::OsmTag &tag) {
    if (tag.Key == "building" || tag.Key == "building:part") {
      return !tag.Value.empty() && tag.Value != "no";
    }
    if (tag.Key == "type" && tag.Value == "building") { return true; }
    return tag.Key == "man_made" &&
           (tag.Value == "chimney" || tag.Value == "tower" || tag.Value == "water_tower");
  });
}

std::expected<std::vector<Data::OsmElementId>, OsmFootprintError>
SelectBuildingRoots(const Data::OsmElements &source) {
  for (const auto &node : source.Nodes()) {
    if (!IsBuilding(node.Tags)) { continue; }
    if (Ambiguous(node.Tags)) {
      return std::unexpected(
          OsmFootprintError{.Code = OsmFootprintErrorCode::AmbiguousTag,
                            .Source = {.Kind = Data::OsmElementKind::Node, .Id = node.Id}});
    }
    if (IsBuilding(node.Tags)) {
      return std::unexpected(
          OsmFootprintError{.Code = OsmFootprintErrorCode::UnsupportedGeometry,
                            .Source = {.Kind = Data::OsmElementKind::Node, .Id = node.Id}});
    }
  }
  std::vector<Data::OsmElementId> roots;
  std::set<uint64_t> relationWays;
  for (const auto &relation : source.Relations()) {
    if (!IsBuilding(relation.Tags)) { continue; }
    if (Ambiguous(relation.Tags)) {
      return std::unexpected(
          OsmFootprintError{.Code = OsmFootprintErrorCode::AmbiguousTag,
                            .Source = {.Kind = Data::OsmElementKind::Relation, .Id = relation.Id}});
    }
    roots.push_back({.Kind = Data::OsmElementKind::Relation, .Id = relation.Id});
    for (const auto &member : relation.Members) {
      if (member.Kind == Data::OsmElementKind::Way) { relationWays.insert(member.Id); }
    }
  }
  for (const auto &way : source.Ways()) {
    if (!IsBuilding(way.Tags)) { continue; }
    if (Ambiguous(way.Tags)) {
      return std::unexpected(
          OsmFootprintError{.Code = OsmFootprintErrorCode::AmbiguousTag,
                            .Source = {.Kind = Data::OsmElementKind::Way, .Id = way.Id}});
    }
    if (!relationWays.contains(way.Id)) {
      roots.push_back({.Kind = Data::OsmElementKind::Way, .Id = way.Id});
    }
  }
  return roots;
}

std::expected<std::vector<uint64_t>, OsmFootprintErrorCode>
CloseRing(std::vector<uint64_t> ring,
          const std::vector<const Data::OsmWay *> &ways,
          const std::map<uint64_t, std::vector<size_t>> &junctions,
          std::vector<bool> &used,
          size_t remaining) {
  while (ring.front() != ring.back()) {
    const auto &incident = junctions.find(ring.back())->second;
    const size_t next = used[incident[0]] ? incident[1] : incident[0];
    if (used[next]) { return std::unexpected(OsmFootprintErrorCode::InvalidRing); }
    used[next] = true;
    const auto &nodes = ways[next]->NodeIds;
    if (nodes.size() - 1 > remaining - (ring.size() - 1)) {
      return std::unexpected(OsmFootprintErrorCode::PointBudgetExceeded);
    }
    if (nodes.front() == ring.back()) {
      ring.insert(ring.end(), nodes.begin() + 1, nodes.end());
    } else {
      ring.insert(ring.end(), nodes.rbegin() + 1, nodes.rend());
    }
  }
  return ring;
}

}

std::span<const Data::OsmTag> OsmBuildingFootprints::Tags(const Building &building) const noexcept {
  if (building.Source.Kind == Data::OsmElementKind::Way) {
    return Source_->Elements.FindWay(building.Source.Id)->Tags;
  }
  return Source_->Elements.FindRelation(building.Source.Id)->Tags;
}

std::expected<void, OsmFootprintError> OsmBuildingFootprints::AppendRing(
    std::span<const uint64_t> nodes, Data::OsmElementId source, bool exterior, size_t maxPoints) {
  if (nodes.size() < 4 || nodes.front() != nodes.back()) {
    return std::unexpected(
        OsmFootprintError{.Code = OsmFootprintErrorCode::InvalidRing, .Source = source});
  }
  nodes = nodes.first(nodes.size() - 1);
  const size_t first = Points_.size() / 2;
  if (nodes.size() > maxPoints - first) {
    return std::unexpected(
        OsmFootprintError{.Code = OsmFootprintErrorCode::PointBudgetExceeded, .Source = source});
  }
  std::vector<uint64_t> unique(nodes.begin(), nodes.end());
  std::ranges::sort(unique);
  if (std::ranges::adjacent_find(unique) != unique.end()) {
    return std::unexpected(
        OsmFootprintError{.Code = OsmFootprintErrorCode::InvalidRing, .Source = source});
  }
  for (const uint64_t id : nodes) {
    const Data::OsmNode &node = *Source_->Elements.FindNode(id);
    Points_.push_back(node.LatitudeDeg);
    Points_.push_back(node.LongitudeDeg);
  }
  Rings_.push_back({.First = static_cast<uint32_t>(first),
                    .Count = static_cast<uint32_t>(nodes.size()),
                    .Exterior = exterior});
  return {};
}

std::expected<void, OsmFootprintError>
OsmBuildingFootprints::AppendRelation(const Data::OsmRelation &relation, size_t maxPoints) {
  const Data::OsmElementId source{.Kind = Data::OsmElementKind::Relation, .Id = relation.Id};
  if (Tag(relation.Tags, "type") != "multipolygon") {
    return std::unexpected(
        OsmFootprintError{.Code = OsmFootprintErrorCode::UnsupportedGeometry, .Source = source});
  }
  std::set<uint64_t> members;
  for (const auto &member : relation.Members) {
    if (!members.insert(member.Id).second) {
      return std::unexpected(
          OsmFootprintError{.Code = OsmFootprintErrorCode::AmbiguousJunction, .Source = source});
    }
  }
  bool hasExterior = false;
  for (const bool exterior : {true, false}) {
    std::vector<const Data::OsmWay *> ways;
    for (const auto &member : relation.Members) {
      if (member.Kind != Data::OsmElementKind::Way ||
          (member.Role != "outer" && member.Role != "inner")) {
        return std::unexpected(OsmFootprintError{.Code = OsmFootprintErrorCode::UnsupportedGeometry,
                                                 .Source = source});
      }
      if ((member.Role == "outer") == exterior) {
        ways.push_back(Source_->Elements.FindWay(member.Id));
      }
    }
    std::ranges::sort(ways, {}, [](const Data::OsmWay *way) { return way->Id; });
    if (auto appended = AppendWays(ways, source, exterior, maxPoints); !appended) {
      return appended;
    }
    hasExterior = hasExterior || (exterior && !ways.empty());
  }
  if (!hasExterior) {
    return std::unexpected(
        OsmFootprintError{.Code = OsmFootprintErrorCode::InvalidRing, .Source = source});
  }
  return {};
}

std::expected<void, OsmFootprintError>
OsmBuildingFootprints::AppendWays(const std::vector<const Data::OsmWay *> &ways,
                                  Data::OsmElementId source,
                                  bool exterior,
                                  size_t maxPoints) {
  std::map<uint64_t, std::vector<size_t>> junctions;
  std::vector<bool> used(ways.size());
  for (size_t at = 0; at < ways.size(); ++at) {
    const auto &nodes = ways[at]->NodeIds;
    if (nodes.size() < 2) {
      return std::unexpected(
          OsmFootprintError{.Code = OsmFootprintErrorCode::InvalidRing, .Source = source});
    }
    if (nodes.front() == nodes.back()) { continue; }
    junctions[nodes.front()].push_back(at);
    junctions[nodes.back()].push_back(at);
  }
  for (const auto &[node, incident] : junctions) {
    (void)node;
    if (incident.size() != 2) {
      return std::unexpected(
          OsmFootprintError{.Code = OsmFootprintErrorCode::AmbiguousJunction, .Source = source});
    }
  }
  for (size_t at = 0; at < ways.size(); ++at) {
    if (used[at]) { continue; }
    const size_t remaining = maxPoints - Points_.size() / 2;
    if (ways[at]->NodeIds.size() - 1 > remaining) {
      return std::unexpected(
          OsmFootprintError{.Code = OsmFootprintErrorCode::PointBudgetExceeded, .Source = source});
    }
    std::vector<uint64_t> ring = ways[at]->NodeIds;
    used[at] = true;
    auto closed = CloseRing(std::move(ring), ways, junctions, used, remaining);
    if (!closed) {
      return std::unexpected(OsmFootprintError{.Code = closed.error(), .Source = source});
    }
    if (auto appended = AppendRing(*closed, source, exterior, maxPoints); !appended) {
      return appended;
    }
  }
  return {};
}

std::expected<OsmBuildingFootprints, OsmFootprintError>
OsmBuildingFootprints::Build(std::shared_ptr<const Data::OsmSourceSnapshot> source,
                             size_t maxPoints) {
  if (!source) {
    return std::unexpected(OsmFootprintError{.Code = OsmFootprintErrorCode::MissingSource});
  }
  maxPoints = std::min(maxPoints, static_cast<size_t>(std::numeric_limits<uint32_t>::max()));
  auto roots = SelectBuildingRoots(source->Elements);
  if (!roots) { return std::unexpected(roots.error()); }
  if (const auto missing = source->Elements.FirstMissingReference(*roots)) {
    return std::unexpected(
        OsmFootprintError{.Code = OsmFootprintErrorCode::MissingReference,
                          .Source = {.Kind = missing->OwnerKind, .Id = missing->OwnerId},
                          .Missing = missing});
  }
  OsmBuildingFootprints result;
  result.Source_ = std::move(source);
  for (const Data::OsmElementId root : *roots) {
    const size_t first = result.Rings_.size();
    auto appended =
        root.Kind == Data::OsmElementKind::Way
            ? result.AppendRing(
                  result.Source_->Elements.FindWay(root.Id)->NodeIds, root, true, maxPoints)
            : result.AppendRelation(*result.Source_->Elements.FindRelation(root.Id), maxPoints);
    if (!appended) { return std::unexpected(appended.error()); }
    result.Buildings_.push_back(
        {.Source = root, .FirstRing = first, .RingCount = result.Rings_.size() - first});
  }
  return result;
}

}
