#ifndef OUTSHINE_WORLD_NAVIGATION_OSMTRANSPORTLOADER_H
#define OUTSHINE_WORLD_NAVIGATION_OSMTRANSPORTLOADER_H

#include <cstddef>
#include <cstdint>
#include <expected>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "OsmElements.h"
#include "Tasks.h"
#include "TransportTopology.h"
#include <world/SourceProvider.h>

namespace outshine::World {

class TransportTopologySnapshot {
  Data::OsmElements Source_;
  TransportTopology Graph_;

public:
  TransportTopologySnapshot(Data::OsmElements source,
                            TransportTopology topology,
                            std::vector<Data::SourceCoverage> coverage,
                            size_t sourceBytes,
                            double readMs,
                            double parseMs,
                            double graphMs)
      : Source_(std::move(source)),
        Graph_(std::move(topology)),
        Coverage(std::move(coverage)),
        SourceBytes(sourceBytes),
        ReadMs(readMs),
        ParseMs(parseMs),
        GraphMs(graphMs) {}

  [[nodiscard]] const Data::OsmSourceIdentity &SourceIdentity() const noexcept {
    return Source_.SourceIdentity();
  }

  [[nodiscard]] const TransportTopology &Topology() const noexcept { return Graph_; }

  [[nodiscard]] std::expected<CircuitRoute, CircuitError>
  ResolveCircuit(uint64_t relationId, std::string_view memberRole = {}) const {
    return Graph_.ResolveCircuit(Source_, relationId, memberRole);
  }

  std::vector<Data::SourceCoverage> Coverage;
  size_t SourceBytes = 0;
  double ReadMs = 0.0;
  double ParseMs = 0.0;
  double GraphMs = 0.0;
};

class OsmTransportLoader {
public:
  enum class Phase : uint8_t { Inactive, Loading, Ready, Failed };

  explicit OsmTransportLoader(Tasks &tasks) : Tasks_(&tasks) {}

  ~OsmTransportLoader();
  OsmTransportLoader(const OsmTransportLoader &) = delete;
  OsmTransportLoader &operator=(const OsmTransportLoader &) = delete;

  [[nodiscard]] std::expected<void, std::string>
  Request(std::span<const Data::SourceProvider> providers, std::string_view shippedRoot);
  void Poll();

  [[nodiscard]] Phase CurrentPhase() const noexcept { return Phase_; }

  [[nodiscard]] std::string_view Error() const noexcept { return Error_; }

  [[nodiscard]] const std::shared_ptr<const TransportTopologySnapshot> &Current() const noexcept {
    return Current_;
  }

  [[nodiscard]] size_t PendingCount() const noexcept { return Pending_.size(); }

private:
  using LoadResult = std::expected<std::shared_ptr<const TransportTopologySnapshot>, std::string>;

  struct Result {
    std::optional<LoadResult> Value;
  };

  struct Pending {
    Tasks::Handle Handle = Tasks::kNoTask;
    uint64_t Revision = 0;
    std::shared_ptr<Result> Output;
  };

  [[nodiscard]] static LoadResult Load(std::span<const Data::SourceProvider> providers,
                                       std::string_view shippedRoot);

  Tasks *Tasks_;
  std::vector<Data::SourceProvider> Requested_;
  std::string Root_;
  std::vector<Pending> Pending_;
  std::shared_ptr<const TransportTopologySnapshot> Current_;
  std::string Error_;
  uint64_t Revision_ = 0;
  Phase Phase_ = Phase::Inactive;
};

}

#endif
