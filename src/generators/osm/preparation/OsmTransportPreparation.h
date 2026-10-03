#ifndef OUTSHINE_GENERATORS_OSM_PREPARATION_OSMTRANSPORTPREPARATION_H
#define OUTSHINE_GENERATORS_OSM_PREPARATION_OSMTRANSPORTPREPARATION_H

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

#include "OsmSourceSnapshot.h"
#include "Tasks.h"
#include "TransportNetworkSnapshot.h"
#include <world/SourceProvider.h>

namespace outshine::Generators::Osm {

struct CircuitRequest {
  std::string Id;
  uint64_t RelationId = 0;

  [[nodiscard]] bool operator==(const CircuitRequest &) const = default;
};

class TransportPreparation {
public:
  enum class Phase : uint8_t { Inactive, Loading, Ready, Failed };

  explicit TransportPreparation(Tasks &tasks) : Tasks_(&tasks) {}

  ~TransportPreparation();
  TransportPreparation(const TransportPreparation &) = delete;
  TransportPreparation &operator=(const TransportPreparation &) = delete;

  [[nodiscard]] std::expected<void, std::string>
  Request(std::span<const Data::SourceProvider> providers,
          std::string_view shippedRoot,
          std::span<const CircuitRequest> routes = {});
  [[nodiscard]] std::expected<void, std::string>
  RequestSource(std::shared_ptr<const SourceSnapshot> source,
                std::span<const CircuitRequest> routes = {});
  void Poll();

  [[nodiscard]] Phase CurrentPhase() const noexcept { return Phase_; }

  [[nodiscard]] std::string_view Error() const noexcept { return Error_; }

  [[nodiscard]] const std::shared_ptr<const World::TransportNetworkSnapshot> &
  Current() const noexcept {
    return Current_;
  }

  [[nodiscard]] const std::shared_ptr<const SourceSnapshot> &Source() const noexcept {
    return CurrentSource_;
  }

  [[nodiscard]] size_t PendingCount() const noexcept { return Pending_ ? 1 : 0; }

  [[nodiscard]] uint64_t CompletedCount() const noexcept { return CompletedCount_; }

  [[nodiscard]] uint64_t CanceledCount() const noexcept { return CanceledCount_; }

private:
  struct Publication {
    std::shared_ptr<const SourceSnapshot> Source;
    std::shared_ptr<const World::TransportNetworkSnapshot> Network;
  };

  using LoadResult = std::expected<Publication, std::string>;

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
                                       std::span<const CircuitRequest> routes,
                                       const std::stop_token &stop);

  [[nodiscard]] static LoadResult BuildSource(std::shared_ptr<const SourceSnapshot> source,
                                              std::span<const CircuitRequest> routes,
                                              const std::stop_token &stop);

  [[nodiscard]] std::expected<void, std::string>
  SetRequest(std::vector<Data::SourceProvider> requested,
             std::string root,
             std::shared_ptr<const SourceSnapshot> source,
             std::span<const CircuitRequest> routes);

  void StartRequested();

  Tasks *Tasks_;
  std::vector<Data::SourceProvider> Requested_;
  std::vector<CircuitRequest> RequestedRoutes_;
  std::string Root_;
  std::shared_ptr<const SourceSnapshot> RequestedSource_;
  std::optional<Pending> Pending_;
  std::shared_ptr<const World::TransportNetworkSnapshot> Current_;
  std::shared_ptr<const SourceSnapshot> CurrentSource_;
  std::string Error_;
  uint64_t Revision_ = 0;
  uint64_t CompletedCount_ = 0;
  uint64_t CanceledCount_ = 0;
  Phase Phase_ = Phase::Inactive;
};

}

#endif
