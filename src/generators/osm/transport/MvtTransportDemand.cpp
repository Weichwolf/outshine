#include "MvtTransportSnapshot.h"
#include "OsmField.h"
#include "StreetField.h"
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <span>
#include <stop_token>
#include <string>
#include <vector>

namespace outshine::Generators::Osm {
std::expected<std::vector<uint32_t>, std::string>
ResolveMvtTransportWays(const MvtTransportSnapshot &snapshot,
                        const OsmField &field,
                        const StreetField &streets,
                        std::span<const World::TransportEdgeId> edges,
                        const std::stop_token &stop) {
  if (stop.stop_requested()) { return std::unexpected("MVT transport demand cancelled"); }
  if (snapshot.Network == nullptr || snapshot.SourceOrigin.get() != field.OriginToken() ||
      snapshot.SourceGeneration != field.Generation() ||
      snapshot.SourceWayCount != streets.Ways().size() || !streets.SourceDigest(field, 0)) {
    return std::unexpected("MVT transport demand does not match its source lease");
  }
  const auto &paths = snapshot.Network->Provenance().Paths;
  if (snapshot.StreetWays.size() != paths.size()) {
    return std::unexpected("MVT transport demand has an incomplete local source binding");
  }
  std::vector<uint32_t> ways;
  ways.reserve(edges.size());
  for (const auto &edge : edges) {
    if (stop.stop_requested()) { return std::unexpected("MVT transport demand cancelled"); }
    if (snapshot.Network->Topology().FindEdge(edge) == nullptr) {
      return std::unexpected("MVT transport demand names an absent directed edge");
    }
    const auto path =
        std::ranges::lower_bound(paths, edge.PathId, {}, &World::TransportPathSource::PathId);
    if (path == paths.end() || path->PathId != edge.PathId) {
      return std::unexpected("MVT transport demand lacks its native path source");
    }
    const auto at = static_cast<size_t>(path - paths.begin());
    const uint32_t way = snapshot.StreetWays[at];
    if (way >= streets.Ways().size() || streets.Ways()[way].PointCount < 2 ||
        !streets.SourceOf(field, way)) {
      return std::unexpected("MVT transport demand has a retired source way");
    }
    ways.push_back(way);
  }
  std::ranges::sort(ways);
  const auto duplicates = std::ranges::unique(ways);
  ways.erase(duplicates.begin(), duplicates.end());
  return ways;
}
}
