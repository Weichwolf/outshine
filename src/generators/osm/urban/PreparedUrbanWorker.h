#ifndef OUTSHINE_GENERATORS_OSM_URBAN_PREPAREDURBANWORKER_H
#define OUTSHINE_GENERATORS_OSM_URBAN_PREPAREDURBANWORKER_H

#include "PreparedUrbanAreas.h"
#include <array>
#include <atomic>
#include <condition_variable>
#include <mutex>
#include <stop_token>
#include <vector>
#include <optional>
#include <variant>

namespace outshine::Generators::Osm {
struct PreparedUrbanAreas::Loaded {
  enum class Phase : uint8_t { Complete, Deferred };
  Phase Stage = Phase::Complete;
  std::shared_ptr<const UrbanArea> Area;
  std::string Key;
};

struct PreparedUrbanAreas::Definition {
  Data::TileId Owner;
  MvtSchema Schema;
  UrbanAreaRecipe Recipe;
};

struct PreparedUrbanAreas::Work {
  enum class Phase : uint8_t { Lookup, Retry, Source, Generate, Publish, Restore };
  Definition Input;
  std::vector<std::weak_ptr<const uint8_t>> Owners;
  std::array<std::unique_ptr<OsmField>, 2> Layouts;
  std::array<std::shared_ptr<const OsmField>, 2> Sources;
  size_t SourceCount = 0;
  Phase Stage = Phase::Lookup;
  Phase Resume = Phase::Lookup;
  std::optional<PreparedAssetWrite> Publication;
  std::atomic_size_t PublicationBytes{0};

  void ClearPublication() noexcept {
    Publication.reset();
    PublicationBytes.store(0, std::memory_order_release);
  }

  std::chrono::steady_clock::time_point RetryAt;
  std::chrono::milliseconds RetryDelay{1};
  std::atomic_bool Running{false};
  std::expected<Loaded, std::string> Result{Loaded{}};
};

struct PreparedUrbanAreas::Worker {
  struct Job {
    Job(std::shared_ptr<Worker> owner, std::shared_ptr<Work> work);
    ~Job();
    Job(const Job &) = delete;
    Job &operator=(const Job &) = delete;
    std::shared_ptr<Worker> Owner;
    std::shared_ptr<Work> Pending;
  };

  Worker(std::shared_ptr<AssetCache> cache, std::string recipe, Budget budget);
  [[nodiscard]] std::string RequestKey(const Definition &input) const;
  [[nodiscard]] std::expected<Loaded, std::string> Load(const std::string &request,
                                                        const Definition &input);
  using Failure = std::variant<AssetCacheError, std::string>;
  [[nodiscard]] std::expected<Loaded, Failure> Read(const std::string &request,
                                                    const Definition &input);
  [[nodiscard]] std::expected<Loaded, std::string> Generate(const std::string &request, Work &work);
  [[nodiscard]] std::expected<Loaded, std::string> Publish(const std::string &request, Work &work);
  [[nodiscard]] static std::expected<Loaded, std::string> ReadFailure(const Failure &error);
  [[nodiscard]] bool Matches(const UrbanArea &area, const Definition &input) const;

  std::shared_ptr<AssetCache> Cache;
  std::string Recipe;
  Budget Limits;
  std::stop_source Stop;
  std::mutex CacheMutex;
  std::mutex CompletionMutex;
  std::condition_variable Completed;
  uint64_t CompletionGeneration = 0;
  std::atomic_uint64_t Hits{0}, Misses{0}, Generations{0}, Writes{0}, ReadBytes{0};
  std::atomic_uint64_t SourcePolls{0}, GeneratedCells{0}, Deferred{0};
  std::atomic_uint64_t PublicationAttempts{0}, PublicationDeferred{0};
};
}
#endif
