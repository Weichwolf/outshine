#ifndef OUTSHINE_GENERATORS_OSM_URBAN_PREPAREDURBANWORKER_H
#define OUTSHINE_GENERATORS_OSM_URBAN_PREPAREDURBANWORKER_H

#include "PreparedUrbanAreas.h"
#include <array>
#include <atomic>
#include <condition_variable>
#include <mutex>
#include <stop_token>
#include <vector>

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
  enum class Phase : uint8_t { Lookup, LookupRetry, Source, Generate };
  Definition Input;
  std::vector<std::weak_ptr<const uint8_t>> Owners;
  std::array<std::unique_ptr<OsmField>, 2> Layouts;
  std::array<std::shared_ptr<const OsmField>, 2> Sources;
  size_t SourceCount = 0;
  Phase Stage = Phase::Lookup;
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

  Worker(std::unique_ptr<AssetCache> cache, std::string recipe, Budget budget);
  [[nodiscard]] std::string RequestKey(const Definition &input) const;
  [[nodiscard]] std::expected<Loaded, std::string> Load(const std::string &request,
                                                        const Definition &input);
  [[nodiscard]] std::expected<Loaded, std::string> Generate(const std::string &request,
                                                            const Work &work);
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
};
}
#endif
