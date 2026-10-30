#include "MvtGroundPortInputsInternal.h"
#include "MvtTransportSourceFacts.h"
#include <cstddef>
#include <cstdint>
#include <expected>
#include <span>
#include <stop_token>
#include <utility>
#include <vector>

namespace outshine::Generators::Osm {
namespace {
using Inputs = MvtGroundFieldCandidateInputs;
using Error = MvtGroundPortInputError;

std::expected<void, Error> Groups(const MvtTransportSourceFacts &facts,
                                  MvtGroundFieldIndex &index,
                                  const std::stop_token &stop) {
  const auto &graph = facts.Network->Topology();
  size_t next = 0;
  for (uint32_t at = 0; at < facts.Groups.size(); ++at) {
    const auto &group = facts.Groups[at];
    if (group.FirstMember != next || group.MemberCount > facts.Members.size() - next) {
      return std::unexpected(Error::InvalidBinding);
    }
    const auto &partition = facts.NodeGroups->Groups[at];
    const auto &ids = facts.NodeGroups->Nodes;
    if (partition.FirstNode > ids.size() ||
        partition.NodeCount > ids.size() - partition.FirstNode) {
      return std::unexpected(Error::InvalidBinding);
    }
    for (const auto &member : std::span(facts.Members).subspan(next, group.MemberCount)) {
      if (stop.stop_requested()) { return std::unexpected(Error::Canceled); }
      const auto *node = graph.FindNode(member.DomainNode);
      if (node == nullptr || member.NodeGroup != at || member.End > 1) {
        return std::unexpected(Error::InvalidBinding);
      }
      ++index.MemberVisits;
      auto &owner = index.Groups[static_cast<size_t>(node - graph.Nodes().data())];
      if (owner == Inputs::MissingGroup) {
        owner = at;
      } else if (owner != at) {
        owner = Inputs::AmbiguousGroup;
      }
    }
    next += group.MemberCount;
  }
  if (next != facts.Members.size()) { return std::unexpected(Error::InvalidBinding); }
  return {};
}
}

std::expected<MvtGroundFieldIndex, MvtGroundPortInputError> IndexMvtGroundFieldFacts(
    const MvtTransportSourceFacts &facts, size_t ways, const std::stop_token &stop) {
  MvtGroundFieldIndex index{.Groups = std::vector<uint32_t>(
                                facts.Network->Topology().Nodes().size(), Inputs::MissingGroup),
                            .Ways = std::vector<uint32_t>(ways, Inputs::MissingGroup)};
  if (auto groups = Groups(facts, index, stop); !groups) { return std::unexpected(groups.error()); }
  for (uint32_t at = 0; at < facts.Ways.size(); ++at) {
    if (stop.stop_requested()) { return std::unexpected(Error::Canceled); }
    const auto &way = facts.Ways[at];
    if (way.StreetWay >= index.Ways.size() || index.Ways[way.StreetWay] != Inputs::MissingGroup ||
        way.PointCount < 2) {
      return std::unexpected(Error::InvalidBinding);
    }
    index.Ways[way.StreetWay] = at;
  }
  return index;
}
}
