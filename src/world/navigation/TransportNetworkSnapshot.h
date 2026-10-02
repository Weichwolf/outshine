#ifndef OUTSHINE_WORLD_NAVIGATION_TRANSPORTNETWORKSNAPSHOT_H
#define OUTSHINE_WORLD_NAVIGATION_TRANSPORTNETWORKSNAPSHOT_H

#include "TransportTopology.h"
#include <world/SourceProvider.h>
#include <algorithm>
#include <cassert>
#include <cstddef>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace outshine::World {

struct TransportLoadMetrics {
  size_t SourceBytes = 0;
  double ReadMs = 0.0;
  double ParseMs = 0.0;
  double GraphMs = 0.0;
};

struct NamedCircuitRoute {
  std::string Id;
  CircuitRoute Circuit;
};

struct ResolvedTransport {
  TransportTopology Graph;
  std::vector<NamedCircuitRoute> Routes;
};

class TransportNetworkSnapshot {
  ResolvedTransport Transport_;
  std::vector<Data::SourceCoverage> Coverage_;
  TransportLoadMetrics Metrics_;

public:
  TransportNetworkSnapshot(ResolvedTransport transport,
                           std::vector<Data::SourceCoverage> coverage,
                           TransportLoadMetrics metrics)
      : Transport_(std::move(transport)), Coverage_(std::move(coverage)), Metrics_(metrics) {
    assert(std::ranges::all_of(Transport_.Routes, [this](const NamedCircuitRoute &route) {
      return route.Circuit.SourceIdentity == Transport_.Graph.SourceIdentity();
    }));
  }

  [[nodiscard]] const Data::SourceIdentity &SourceIdentity() const noexcept {
    return Transport_.Graph.SourceIdentity();
  }

  [[nodiscard]] const TransportTopology &Topology() const noexcept { return Transport_.Graph; }

  [[nodiscard]] const CircuitRoute *FindRoute(std::string_view id) const noexcept {
    for (const NamedCircuitRoute &route : Transport_.Routes) {
      if (route.Id == id) { return &route.Circuit; }
    }
    return nullptr;
  }

  [[nodiscard]] size_t RouteCount() const noexcept { return Transport_.Routes.size(); }

  [[nodiscard]] std::span<const NamedCircuitRoute> Routes() const noexcept {
    return Transport_.Routes;
  }

  [[nodiscard]] size_t RouteEdgeCount() const noexcept {
    size_t edges = 0;
    for (const NamedCircuitRoute &route : Transport_.Routes) {
      edges += route.Circuit.EdgeIds.size();
    }
    return edges;
  }

  [[nodiscard]] std::span<const Data::SourceCoverage> Coverage() const noexcept {
    return Coverage_;
  }

  [[nodiscard]] const TransportLoadMetrics &Metrics() const noexcept { return Metrics_; }
};

}

#endif
