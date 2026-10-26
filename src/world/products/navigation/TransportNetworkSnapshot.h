#ifndef OUTSHINE_WORLD_PRODUCTS_NAVIGATION_TRANSPORTNETWORKSNAPSHOT_H
#define OUTSHINE_WORLD_PRODUCTS_NAVIGATION_TRANSPORTNETWORKSNAPSHOT_H

#include "TransportTopology.h"
#include "TransportProvenance.h"
#include "GeographicBounds.h"
#include <algorithm>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <expected>
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
  std::vector<GeographicBounds> Coverage_;
  TransportLoadMetrics Metrics_;
  TransportProvenance Provenance_;

public:
  TransportNetworkSnapshot(ResolvedTransport transport,
                           std::vector<GeographicBounds> coverage,
                           TransportLoadMetrics metrics,
                           TransportProvenance provenance = {})
      : Transport_(std::move(transport)),
        Coverage_(std::move(coverage)),
        Metrics_(metrics),
        Provenance_(std::move(provenance)) {
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

  [[nodiscard]] std::span<const GeographicBounds> Coverage() const noexcept { return Coverage_; }

  [[nodiscard]] const TransportLoadMetrics &Metrics() const noexcept { return Metrics_; }

  [[nodiscard]] const TransportProvenance &Provenance() const noexcept { return Provenance_; }

  [[nodiscard]] size_t CapacityBytes() const noexcept;
};

enum class TransportNetworkError : uint8_t {
  InvalidTopology,
  InvalidCoverage,
  InvalidRoutes,
  InvalidProvenance
};

[[nodiscard]] std::expected<void, TransportNetworkError>
ValidateTransportNetwork(const TransportNetworkSnapshot &network);

}

#endif
