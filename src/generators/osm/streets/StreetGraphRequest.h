#ifndef OUTSHINE_GENERATORS_OSM_STREETS_STREETGRAPHREQUEST_H
#define OUTSHINE_GENERATORS_OSM_STREETS_STREETGRAPHREQUEST_H

#include <cstddef>
#include <expected>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <stop_token>
#include <string>

#include "StreetGraphBuilder.h"
#include "Tasks.h"
#include "PreparedStreetGraph.h"

namespace outshine::Generators::Osm {

class StreetGraphTopology {
public:
  StreetGraphTopology(std::shared_ptr<const OsmField> vectors, StreetField ways);

  [[nodiscard]] bool Matches(const std::shared_ptr<const OsmField> &vectors,
                             const StreetField &ways) const noexcept;

  [[nodiscard]] const OsmField &Vectors() const noexcept { return *Vectors_; }

  [[nodiscard]] const StreetField &Ways() const noexcept { return Ways_; }

private:
  friend class StreetGraphRequest;
  [[nodiscard]] std::expected<StreetGraphBuildJob, std::string>
  Begin(Path::Network::HeightSource heightOf, const Path::Network *retained);
  [[nodiscard]] std::optional<StreetGraphBuildJob> Take();
  void Save(StreetGraphBuildJob job);

  std::shared_ptr<const OsmField> Vectors_;
  StreetField Ways_;
  std::mutex Mutex_;
  std::optional<StreetGraphBuildJob> Pending_;
};

class StreetGraphRequest {
public:
  struct Completed {
    StreetGraphBuilder::Built Graph;
    double LongestSliceMs = 0.0;
    double WorkerMs = 0.0;
    bool CacheHit = false;
    size_t ReadBytes = 0;
  };

  using JobFactory = std::function<std::expected<StreetGraphBuildJob, std::string>()>;
  using Resolver = std::function<PreparedStreetGraph::Result(const PreparedStreetGraph::Factory &,
                                                             const std::stop_token &)>;

  using KeyFactory = std::function<std::expected<std::string, std::string>()>;
  [[nodiscard]] static Resolver CachedResolver(std::shared_ptr<PreparedStreetGraph> cache,
                                               KeyFactory key);

  // The pool outlives this owner and every unfinished callback.
  StreetGraphRequest(Tasks &pool, StreetGraphBuildJob job);
  StreetGraphRequest(Tasks &pool, JobFactory factory, Resolver resolver = {});
  StreetGraphRequest(Tasks &pool,
                     std::shared_ptr<StreetGraphTopology> topology,
                     Path::Network::HeightSource heightOf,
                     Resolver resolver,
                     std::shared_ptr<const Path::Network> retained = {});
  ~StreetGraphRequest();
  StreetGraphRequest(const StreetGraphRequest &) = delete;
  StreetGraphRequest &operator=(const StreetGraphRequest &) = delete;

  void Cancel() noexcept;
  [[nodiscard]] bool Complete() const noexcept;
  [[nodiscard]] const char *PhaseName() const noexcept;
  [[nodiscard]] size_t Advances() const noexcept;
  [[nodiscard]] std::optional<std::expected<Completed, std::string>> Collect();

private:
  struct Work;
  void Start(Tasks &pool);
  static std::expected<StreetGraphBuilder::Built, std::string>
  Generate(Work &work, const std::stop_token &stop);
  static void Run(Work &work, const std::stop_token &stop);
  Tasks &Pool_;
  std::shared_ptr<Work> Work_;
  std::stop_source Stop_;
};

}

#endif
