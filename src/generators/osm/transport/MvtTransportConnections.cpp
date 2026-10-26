#include "MvtTransportSnapshotInternal.h"
#include "Wayfinding.h"
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <stop_token>
#include <string>
#include <tuple>

namespace outshine::Generators::Osm::MvtTransport {
std::expected<Connections, std::string>
ReadConnections(const Sources &sources, const Path::Network &network, const std::stop_token &stop) {
  Connections out;
  out.Segments.reserve(network.EdgeCount() / 2);
  for (size_t from = 0; from < network.NodeCount(); ++from) {
    if (stop.stop_requested()) { return std::unexpected("MVT transport normalization cancelled"); }
    for (const auto &edge : network.ConnectionsFrom(from)) {
      ++out.InputCount;
      if (edge.Source.Way >= network.WayCount() || edge.Source.Step() == 0 ||
          edge.To >= network.NodeCount()) {
        return std::unexpected("MVT transport connection lacks a valid source segment");
      }
      const size_t way = network.TagOf(edge.Source.Way);
      if (way >= sources.Ways.size()) {
        return std::unexpected("MVT transport graph does not match the street field");
      }
      if (edge.Source.Step() >= sources.Ways[way].PointCount) {
        return std::unexpected("MVT transport connection exceeds its actual source ring");
      }
      if (edge.Source.Reverse() || !sources.Ways[way].Usable || edge.To == from) { continue; }
      const uint32_t step = edge.Source.Step() - 1;
      out.Segments.push_back({.Key = SegmentSignature(sources.Ways[way].Key, step),
                              .Way = way,
                              .From = from,
                              .To = edge.To,
                              .SourceStep = step});
    }
  }
  const auto key = [](const Segment &segment) {
    return std::tie(segment.Key, segment.From, segment.To);
  };
  std::ranges::sort(out.Segments, {}, key);
  const auto duplicates = std::ranges::unique(out.Segments, {}, key);
  out.Segments.erase(duplicates.begin(), duplicates.end());
  return out;
}
}
