#ifndef OUTSHINE_ENGINE_STREAMING_STREETGRAPHPREPARATION_H
#define OUTSHINE_ENGINE_STREAMING_STREETGRAPHPREPARATION_H

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

namespace outshine {

class StreetGraphPreparation {
public:
  struct Completed {
    Generators::Osm::StreetGraphBuilder::Built Graph;
    double LongestSliceMs = 0.0;
    double WorkerMs = 0.0;
    bool CacheHit = false;
    size_t ReadBytes = 0;
  };

  using JobFactory =
      std::function<std::expected<Generators::Osm::StreetGraphBuildJob, std::string>()>;
  using Resolver = std::function<Generators::Osm::PreparedStreetGraph::Result(
      const Generators::Osm::PreparedStreetGraph::Factory &, const std::stop_token &)>;

  using KeyFactory = std::function<std::expected<std::string, std::string>()>;
  [[nodiscard]] static Resolver
  CachedResolver(std::shared_ptr<Generators::Osm::PreparedStreetGraph> cache, KeyFactory key);

  StreetGraphPreparation(Tasks &pool, Generators::Osm::StreetGraphBuildJob job);
  StreetGraphPreparation(Tasks &pool, JobFactory factory, Resolver resolver = {});
  ~StreetGraphPreparation();
  StreetGraphPreparation(const StreetGraphPreparation &) = delete;
  StreetGraphPreparation &operator=(const StreetGraphPreparation &) = delete;

  void Cancel() noexcept;
  [[nodiscard]] bool Complete() const noexcept;
  [[nodiscard]] const char *PhaseName() const noexcept;
  [[nodiscard]] size_t Advances() const noexcept;
  [[nodiscard]] std::optional<std::expected<Completed, std::string>> Collect();

private:
  struct Work;
  void Start(Tasks &pool);
  static std::expected<Generators::Osm::StreetGraphBuilder::Built, std::string>
  Generate(Work &work, const std::stop_token &stop);
  static void Run(Work &work, const std::stop_token &stop);
  Tasks &Pool_;
  std::shared_ptr<Work> Work_;
  std::stop_source Stop_;
};

}

#endif
