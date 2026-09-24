#ifndef OUTSHINE_WORLD_NAVIGATION_OSMTRANSPORTLOADER_H
#define OUTSHINE_WORLD_NAVIGATION_OSMTRANSPORTLOADER_H

#include <cstddef>
#include <algorithm>
#include <cassert>
#include <cstdint>
#include <expected>
#include <memory>
#include <optional>
#include <span>
#include <stop_token>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "OsmElements.h"
#include "Tasks.h"
#include "TransportTopology.h"
#include <world/SourceProvider.h>

namespace outshine::World {

struct TransportLoadMetrics {
  size_t SourceBytes = 0;
  double ReadMs = 0.0;
  double ParseMs = 0.0;
  double GraphMs = 0.0;
};

struct OsmCircuitRequest {
  std::string Id;
  uint64_t RelationId = 0;

  [[nodiscard]] bool operator==(const OsmCircuitRequest &) const = default;
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
  Data::OsmElements Source_;
  ResolvedTransport Transport_;
  std::vector<Data::SourceCoverage> Coverage_;
  TransportLoadMetrics Metrics_;

public:
  TransportNetworkSnapshot(Data::OsmElements source,
                           ResolvedTransport transport,
                           std::vector<Data::SourceCoverage> coverage,
                           TransportLoadMetrics metrics)
      : Source_(std::move(source)),
        Transport_(std::move(transport)),
        Coverage_(std::move(coverage)),
        Metrics_(metrics) {
    assert(Source_.SourceIdentity() == Transport_.Graph.SourceIdentity());
    assert(std::ranges::all_of(Transport_.Routes, [this](const NamedCircuitRoute &route) {
      return route.Circuit.SourceIdentity == Source_.SourceIdentity();
    }));
  }

  [[nodiscard]] const Data::OsmSourceIdentity &SourceIdentity() const noexcept {
    return Source_.SourceIdentity();
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

  [[nodiscard]] std::expected<CircuitRoute, CircuitError>
  ResolveCircuit(uint64_t relationId, std::string_view memberRole = {}) const {
    return Transport_.Graph.ResolveCircuit(Source_, relationId, memberRole);
  }
};

class OsmTransportLoader {
public:
  enum class Phase : uint8_t { Inactive, Loading, Ready, Failed };

  explicit OsmTransportLoader(Tasks &tasks) : Tasks_(&tasks) {}

  ~OsmTransportLoader();
  OsmTransportLoader(const OsmTransportLoader &) = delete;
  OsmTransportLoader &operator=(const OsmTransportLoader &) = delete;

  [[nodiscard]] std::expected<void, std::string>
  Request(std::span<const Data::SourceProvider> providers,
          std::string_view shippedRoot,
          std::span<const OsmCircuitRequest> routes = {});
  void Poll();

  [[nodiscard]] Phase CurrentPhase() const noexcept { return Phase_; }

  [[nodiscard]] std::string_view Error() const noexcept { return Error_; }

  [[nodiscard]] const std::shared_ptr<const TransportNetworkSnapshot> &Current() const noexcept {
    return Current_;
  }

  [[nodiscard]] size_t PendingCount() const noexcept { return Pending_ ? 1 : 0; }

  [[nodiscard]] uint64_t CompletedCount() const noexcept { return CompletedCount_; }

  [[nodiscard]] uint64_t CanceledCount() const noexcept { return CanceledCount_; }

private:
  using LoadResult = std::expected<std::shared_ptr<const TransportNetworkSnapshot>, std::string>;

  struct Result {
    std::optional<LoadResult> Value;
  };

  struct Pending {
    Tasks::Handle Handle = Tasks::kNoTask;
    uint64_t Revision = 0;
    std::shared_ptr<Result> Output;
    std::stop_source Stop;
  };

  [[nodiscard]] static LoadResult Load(std::span<const Data::SourceProvider> providers,
                                       std::string_view shippedRoot,
                                       std::span<const OsmCircuitRequest> routes,
                                       const std::stop_token &stop);

  void StartRequested();

  Tasks *Tasks_;
  std::vector<Data::SourceProvider> Requested_;
  std::vector<OsmCircuitRequest> RequestedRoutes_;
  std::string Root_;
  std::optional<Pending> Pending_;
  std::shared_ptr<const TransportNetworkSnapshot> Current_;
  std::string Error_;
  uint64_t Revision_ = 0;
  uint64_t CompletedCount_ = 0;
  uint64_t CanceledCount_ = 0;
  Phase Phase_ = Phase::Inactive;
};

}

#endif
