#ifndef OUTSHINE_ENGINE_STREAMING_GROUNDREGIONPREPARATION_H
#define OUTSHINE_ENGINE_STREAMING_GROUNDREGIONPREPARATION_H

#include "PreparedGroundRegions.h"
#include "Tasks.h"
#include <expected>
#include <functional>
#include <memory>
#include <optional>
#include <stop_token>
#include <string>

namespace outshine {
class GroundRegionPreparation {
public:
  struct Completed {
    std::string Key;
    std::optional<Generators::Osm::PreparedGroundRegions::Loaded> Loaded;
    double WorkerMs = 0;
  };

  using Factory = std::function<std::expected<Completed, std::string>(std::stop_token)>;

  GroundRegionPreparation(Tasks &pool, Factory factory);
  ~GroundRegionPreparation();
  GroundRegionPreparation(const GroundRegionPreparation &) = delete;
  GroundRegionPreparation &operator=(const GroundRegionPreparation &) = delete;
  [[nodiscard]] std::optional<std::expected<Completed, std::string>> Collect();

private:
  struct Work;
  std::shared_ptr<Work> Work_;
  std::stop_source Stop_;
};
}
#endif
