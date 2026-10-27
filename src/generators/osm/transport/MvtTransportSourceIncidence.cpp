#include "MvtTransportSourceFactsInternal.h"
#include "Wayfinding.h"
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <span>
#include <stop_token>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

namespace outshine::Generators::Osm::MvtTransport {
namespace {
constexpr uint64_t kNoGroup = 0;

struct RawSpan {
  size_t From = 0;
  size_t To = 0;
  uint32_t Way = 0;
  uint32_t Step = 0;
};

bool Consume(size_t count, size_t width, size_t &remaining) {
  if (count > remaining / width) { return false; }
  remaining -= count * width;
  return true;
}

std::vector<uint64_t> MapSourceNodeGroups(const SourceFactInput &input,
                                          const MvtTransportNodeGroups &groups) {
  std::vector<uint64_t> mapped(input.Network.NodeCount(), kNoGroup);
  for (const auto &segment : input.Segments) {
    auto &from = mapped[segment.From];
    auto &to = mapped[segment.To];
    from = from == 0 ? segment.FromId : std::min(from, segment.FromId);
    to = to == 0 ? segment.ToId : std::min(to, segment.ToId);
  }
  const auto first = [&groups](const MvtTransportNodeGroup &group) {
    return groups.Nodes[group.FirstNode];
  };
  for (auto &id : mapped) {
    if (id == kNoGroup) { continue; }
    const auto at = std::ranges::lower_bound(groups.Groups, id, {}, first);
    id = static_cast<uint64_t>(at - groups.Groups.begin()) + 1;
  }
  return mapped;
}

std::expected<std::vector<RawSpan>, std::string> OriginalSpans(const Path::Network &network,
                                                               const std::stop_token &stop) {
  std::vector<RawSpan> spans;
  spans.reserve(network.EdgeCount());
  for (size_t from = 0; from < network.NodeCount(); ++from) {
    if (stop.stop_requested()) { return std::unexpected("MVT transport normalization cancelled"); }
    for (const auto &edge : network.ConnectionsFrom(from)) {
      spans.push_back({.From = edge.Source.Reverse() ? edge.To : from,
                       .To = edge.Source.Reverse() ? from : edge.To,
                       .Way = edge.Source.Way,
                       .Step = edge.Source.Step() - 1});
    }
  }
  const auto key = [](const RawSpan &span) {
    return std::tie(span.Way, span.Step, span.From, span.To);
  };
  std::ranges::sort(spans, {}, key);
  const auto duplicate = std::ranges::unique(spans, {}, key);
  spans.erase(duplicate.begin(), duplicate.end());
  return spans;
}

void AddMembers(MvtTransportSourceFacts &facts,
                const SourceFactInput &input,
                std::span<const uint64_t> mapped) {
  facts.Members.reserve(input.Segments.size() * 2);
  for (const auto &segment : input.Segments) {
    const auto &way = input.SourceRows.Ways[segment.Way];
    const bool reverse = way.Attributes.Travel == OsmWayTravel::Reverse;
    for (const bool end : {false, true}) {
      facts.Members.push_back(
          {.Edge = {.PathId = Identifier(segment.Key),
                    .SegmentOrdinal = segment.FragmentOrdinal,
                    .Direction =
                        reverse ? World::EdgeDirection::Reverse : World::EdgeDirection::Forward},
           .DomainNode = end ? segment.ToId : segment.FromId,
           .NodeGroup = static_cast<uint32_t>(mapped[end ? segment.To : segment.From] - 1),
           .StreetWay = static_cast<uint32_t>(segment.Way),
           .SourceStep = segment.SourceStep,
           .End = static_cast<uint8_t>(end != reverse)});
    }
  }
  std::ranges::sort(facts.Members, {}, [](const MvtTransportSourceMember &member) {
    return std::tie(member.NodeGroup, member.Edge, member.End);
  });
  for (size_t at = 0; at < facts.Members.size(); ++at) {
    auto &group = facts.Groups[facts.Members[at].NodeGroup];
    if (group.MemberCount == 0) { group.FirstMember = static_cast<uint32_t>(at); }
    ++group.MemberCount;
  }
}

}

bool FitsSourceFacts(const SourceFactInput &input, size_t groups, size_t bytesMost) {
  return Consume(1, sizeof(MvtTransportSourceFacts), bytesMost) &&
         Consume(groups, sizeof(MvtTransportSourceGroup), bytesMost) &&
         Consume(input.Segments.size(), 2 * sizeof(MvtTransportSourceMember), bytesMost) &&
         Consume(input.SourceRows.StreetWays.size(), sizeof(MvtTransportWayFacts), bytesMost) &&
         Consume(input.SourceRows.StreetWays.size(), sizeof(uint32_t), bytesMost) &&
         Consume(input.Network.NodeCount(), sizeof(uint64_t), bytesMost) &&
         Consume(input.Network.EdgeCount(), sizeof(RawSpan), bytesMost);
}

std::expected<void, std::string> FillSourceFacts(MvtTransportSourceFacts &facts,
                                                 const SourceFactInput &input,
                                                 const std::stop_token &stop) {
  const auto mapped = MapSourceNodeGroups(input, *facts.NodeGroups);
  auto raw = OriginalSpans(input.Network, stop);
  if (!raw) { return std::unexpected(std::move(raw.error())); }
  facts.Groups.resize(facts.NodeGroups->Groups.size());
  for (const auto &span : *raw) {
    for (const size_t end : {span.From, span.To}) {
      if (mapped[end] != kNoGroup) { ++facts.Groups[mapped[end] - 1].OriginalEndpointCount; }
    }
  }
  AddMembers(facts, input, mapped);
  for (auto &group : facts.Groups) {
    group.Complete = group.OriginalEndpointCount == group.MemberCount;
    group.FilteredEndpointCount = group.OriginalEndpointCount > group.MemberCount
                                      ? group.OriginalEndpointCount - group.MemberCount
                                      : 0;
  }
  return FillSourceWayFacts(facts, input, stop);
}
}
