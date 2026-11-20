#ifndef OUTSHINE_ENGINE_REGION_ROADPAGECONSTRUCTION_H
#define OUTSHINE_ENGINE_REGION_ROADPAGECONSTRUCTION_H

#include "Corridors.h"
#include "RoadPageQueue.h"
#include "Geometry.h"
#include <chrono>
#include <cstddef>
#include <expected>
#include <memory>
#include <string>
#include <unordered_set>
#include <vector>

namespace outshine {
class RoadPageConstruction {
public:
  [[nodiscard]] static std::expected<std::unique_ptr<RoadPageConstruction>, std::string>
  Begin(Tasks &pool,
        std::shared_ptr<Generators::PreparedRoadPages> pages,
        const Generators::Corridors::Job &plan,
        const Generators::Corridors::Site &site,
        const std::string &planKey,
        std::shared_ptr<AssetPipeline> publications);
  [[nodiscard]] std::expected<bool, std::string> Advance(const Generators::Corridors::Job &plan,
                                                         const Generators::Corridors::Site &site);
  [[nodiscard]] std::expected<Generators::RoadPageManifest, std::string> TakeManifest();

  [[nodiscard]] size_t Pages() const noexcept { return PageCount_; }

  [[nodiscard]] size_t ExpectedPages() const noexcept { return Pages_.size(); }

  [[nodiscard]] size_t MissRequests() const noexcept { return MissRequests_; }

  [[nodiscard]] size_t NativeBytes() const noexcept { return NativeBytes_; }

private:
  enum class Stage : unsigned char { Lookup, NeedsBake, Baking };

  struct Pending {
    size_t Index;
    std::string Key;
    Generators::RoadPageAddress Address;
    Stage Phase = Stage::Lookup;
    std::shared_ptr<const Generators::RoadPageProducer> Producer;
  };

  RoadPageConstruction(Tasks &pool,
                       std::shared_ptr<Generators::PreparedRoadPages> pages,
                       std::shared_ptr<AssetPipeline> publications);
  [[nodiscard]] std::expected<Pending, std::string>
  PreparePage(const Generators::Corridors::Job &plan,
              const Generators::Corridors::Site &site,
              size_t index);
  [[nodiscard]] std::expected<void, std::string>
  RequestLookups(const Generators::Corridors::Job &plan,
                 const Generators::Corridors::Site &site,
                 std::chrono::steady_clock::time_point until);
  [[nodiscard]] std::expected<void, std::string>
  RequestBakes(const Generators::Corridors::Job &plan,
               const Generators::Corridors::Site &site,
               std::chrono::steady_clock::time_point until);
  [[nodiscard]] std::expected<std::shared_ptr<const Generators::RoadPageProducer>, std::string>
  PrepareBake(const Generators::Corridors::Job &plan,
              const Generators::Corridors::Site &site,
              const Pending &pending) const;
  [[nodiscard]] std::expected<void, std::string> Collect();
  [[nodiscard]] std::expected<void, std::string>
  SavePage(Pending &pending, const RoadPageQueue::Completed &completed);
  static constexpr size_t PageBytesMost = size_t{64} * 1024u * 1024u;
  static constexpr size_t PartsMost = 64;
  size_t BakesMost_;
  size_t PendingMost_;
  RoadPageQueue Queue_;
  std::vector<std::vector<Generators::RoadPagePart>> Pages_;
  std::vector<Pending> Pending_;
  std::unordered_set<std::string> UniqueShapes_;
  Generators::RoadPageManifest Manifest_;
  size_t Requested_ = 0;
  size_t PageCount_ = 0;
  size_t MissRequests_ = 0;
  size_t NativeBytes_ = 0;
  bool ManifestTaken_ = false;
};
}
#endif
