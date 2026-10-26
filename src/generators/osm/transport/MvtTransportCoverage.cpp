#include "MvtTransportSnapshotInternal.h"
#include <algorithm>
#include <cstdint>
#include <expected>
#include <numeric>
#include <stop_token>
#include <string>
#include <tuple>
#include <vector>

namespace outshine::Generators::Osm::MvtTransport {
std::expected<void, std::string> RecordMissingTopology(Sources &sources,
                                                       const std::stop_token &stop) {
  if (stop.stop_requested()) { return std::unexpected("MVT transport normalization cancelled"); }
  const auto &references = sources.Provenance.References;
  std::vector<uint32_t> order(references.size());
  std::iota(order.begin(), order.end(), uint32_t{0});
  const auto key = [&references](uint32_t at) {
    return std::tie(references[at].Source, references[at].Fragment);
  };
  std::ranges::sort(order, {}, key);
  for (const auto &way : sources.Ways) {
    if (stop.stop_requested()) { return std::unexpected("MVT transport normalization cancelled"); }
    if (!way.Usable) { continue; }
    const auto requested = std::tie(way.Source.Source, way.Key);
    const auto at = std::ranges::lower_bound(order, requested, {}, key);
    if (at == order.end() || key(*at) != requested) {
      sources.Omissions.push_back({.Source = way.Source.Source,
                                   .Feature = way.Feature,
                                   .Reason = MvtTransportOmissionReason::MissingTopology});
    }
  }
  return {};
}
}
