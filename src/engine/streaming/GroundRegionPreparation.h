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

namespace outshine::Generators {
class PreparedRoadPages;
struct RoadPageManifest;
}

namespace outshine {
class GroundRegionPreparation {
public:
  struct Completed {
    std::string Key;
    std::optional<Generators::Osm::PreparedGroundRegions::Loaded> Loaded;
    double WorkerMs = 0;
    std::string RequestKey;
    bool RequestHit = false;
    std::shared_ptr<const Generators::RoadPageManifest> SurfacePages;
  };

  using Failure = Generators::Osm::PreparedGroundRegions::Failure;
  using Attempt = std::expected<Completed, Failure>;
  using Factory = std::function<Attempt(const std::stop_token &)>;
  using KeyFactory = std::function<std::expected<std::string, std::string>()>;

  struct StoreInputs {
    std::shared_ptr<Generators::Osm::PreparedGroundRegions> Assets;
    std::string Key, RequestKey;
    std::shared_ptr<const GroundRegionAsset> Region;
    std::shared_ptr<const Generators::WaterAsset> Water;
    std::shared_ptr<const Generators::RoadPageManifest> Manifest;
    std::shared_ptr<Generators::PreparedRoadPages> Pages;
  };

  [[nodiscard]] static Factory
  Lookup(std::shared_ptr<Generators::Osm::PreparedGroundRegions> assets,
         std::string request,
         KeyFactory key = {},
         bool checked = false);
  [[nodiscard]] static Factory PrepareStore(StoreInputs inputs);

  GroundRegionPreparation(Tasks &pool,
                          Factory factory,
                          std::shared_ptr<Generators::PreparedRoadPages> pages = {});
  ~GroundRegionPreparation();
  GroundRegionPreparation(const GroundRegionPreparation &) = delete;
  GroundRegionPreparation &operator=(const GroundRegionPreparation &) = delete;
  void Advance();
  void Cancel() noexcept;
  [[nodiscard]] std::optional<std::expected<Completed, std::string>> Collect();
  [[nodiscard]] const std::expected<Completed, std::string> *Peek() const noexcept;

private:
  struct Work;
  void Post();
  static void Run(Work &work, const std::stop_token &stop);
  Tasks &Pool_;
  std::shared_ptr<Work> Work_;
  std::stop_source Stop_;
};
}
#endif
