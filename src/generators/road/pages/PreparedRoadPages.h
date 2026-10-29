#ifndef OUTSHINE_GENERATORS_ROAD_PAGES_PREPAREDROADPAGES_H
#define OUTSHINE_GENERATORS_ROAD_PAGES_PREPAREDROADPAGES_H

#include "RoadPageAsset.h"
#include "RoadPageManifest.h"
#include "AssetCache.h"
#include "RoadPageProduct.h"
#include "RoadPageProducer.h"
#include <cstddef>
#include <cstdint>
#include <expected>
#include <stop_token>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <variant>
#include <utility>
#include <vector>

namespace outshine::Generators {
class PreparedRoadPages {
public:
  static constexpr size_t ManifestBytesMost = size_t{8} * 1024u * 1024u;

  using Failure = std::variant<AssetCacheError, std::string>;

  class PageWrite {
  public:
    struct Counters {
      size_t MetadataReads = 0, Preparations = 0, Publications = 0;
    };

    [[nodiscard]] Counters Costs() const noexcept { return Costs_; }

    [[nodiscard]] bool NeedsProducer() const noexcept {
      return Phase_ == Phase::Lookup || Phase_ == Phase::Generate;
    }

    [[nodiscard]] size_t PublicationBytes() const noexcept {
      return Package_ ? Package_->CapacityBytes() : 0;
    }

  private:
    friend class PreparedRoadPages;
    enum class Phase { Lookup, Generate, Check, Publish, Restore, Bind, Complete };

    PageWrite(RoadPageRequest request, std::string key)
        : Request_(std::move(request)), Key_(std::move(key)) {}

    RoadPageRequest Request_;
    std::string Key_;
    Phase Phase_ = Phase::Lookup;
    std::optional<PreparedAssetWrite> Package_;
    AssetRecord Record_;
    Counters Costs_;
  };

  [[nodiscard]] static std::string DescribeFailure(const Failure &failure);
  [[nodiscard]] static std::expected<PageWrite, Failure> PreparePage(RoadPageRequest request);
  [[nodiscard]] std::expected<AssetRecord, Failure>
  Prepare(PageWrite &write,
          const RoadPageProducer *producer = nullptr,
          const std::stop_token &stop = {});

  class ManifestWrite {
  public:
    [[nodiscard]] const std::string &Key() const noexcept { return Key_; }

    // Prepared bytes; the dependency snapshot has an independent owner until checked.
    [[nodiscard]] size_t PublicationBytes() const noexcept { return Package_.CapacityBytes(); }

  private:
    friend class PreparedRoadPages;

    struct Dependency {
      std::string Key;
      uint64_t ByteCount = 0;
      uint32_t Level = 0;
    };

    ManifestWrite(std::string key, PreparedAssetWrite package, std::vector<Dependency> dependencies)
        : Dependencies_(std::move(dependencies)),
          Key_(std::move(key)),
          Package_(std::move(package)) {}

    std::vector<Dependency> Dependencies_;
    std::string Key_;
    PreparedAssetWrite Package_;
    size_t NextDependency_ = 0;
    bool DependenciesReady_ = false, Committed_ = false;
  };

  [[nodiscard]] std::expected<ManifestWrite, Failure>
  PrepareManifest(const RoadPageManifest &manifest);
  [[nodiscard]] std::expected<std::string, Failure> StoreManifest(ManifestWrite &write,
                                                                  const std::stop_token &stop = {});
  [[nodiscard]] std::expected<std::optional<std::shared_ptr<const RoadPageManifest>>, Failure>
  LoadManifest(const std::string &key);
  [[nodiscard]] std::expected<void, std::string> InvalidateManifest(const std::string &key);

  [[nodiscard]] static std::expected<std::unique_ptr<PreparedRoadPages>, std::string>
  Open(const std::string &directory);
  [[nodiscard]] std::expected<std::optional<AssetRecord>, std::string>
  Inspect(const RoadPageAddress &address, size_t bytesMost);
  [[nodiscard]] std::expected<std::optional<RoadPageLease>, std::string>
  Load(const RoadPageAddress &address, size_t bytesMost);
  [[nodiscard]] std::expected<std::optional<RoadPageLease>, std::string>
  LoadKey(const std::string &key, size_t bytesMost);
  [[nodiscard]] std::expected<std::optional<RoadPageLease>, std::string>
  LoadRequest(const std::string &requestKey, size_t bytesMost);
  [[nodiscard]] std::expected<AssetRecord, std::string>
  Prepare(const RoadPageRequest &request,
          const RoadPageProducer *producer = nullptr,
          const std::stop_token &stop = {});

private:
  explicit PreparedRoadPages(std::unique_ptr<AssetCache> cache);
  [[nodiscard]] std::expected<void, Failure> ResolvePageRecord(PageWrite &write);
  [[nodiscard]] std::expected<void, Failure>
  GeneratePage(PageWrite &write, const RoadPageProducer *producer, const std::stop_token &stop);
  [[nodiscard]] std::expected<std::optional<RoadPageLease>, std::string>
  Read(const std::string &key, size_t bytesMost, const std::string &requestKey = {});
  [[nodiscard]] std::expected<std::optional<AssetRecord>, Failure>
  ReadRecord(const std::string &key, const RoadPageAddress &address, size_t bytesMost);
  std::unique_ptr<AssetCache> Cache_;
  std::mutex Lock_;
};
}
#endif
