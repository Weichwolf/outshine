#ifndef OUTSHINE_GENERATORS_OSM_STREETS_STREETGRAPHREQUEST_H
#define OUTSHINE_GENERATORS_OSM_STREETS_STREETGRAPHREQUEST_H

#include <cstddef>
#include <expected>
#include <functional>
#include <memory>
#include <optional>
#include <stop_token>
#include <string>

#include "StreetGraphBuilder.h"
#include "Tasks.h"
#include "PreparedStreetGraph.h"

namespace outshine::Generators::Osm {

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
