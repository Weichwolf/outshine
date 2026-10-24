#ifndef OUTSHINE_GENERATORS_ROAD_PREPAREDROADPAGES_H
#define OUTSHINE_GENERATORS_ROAD_PREPAREDROADPAGES_H

#include "RoadPageAsset.h"
#include "RoadPageManifest.h"
#include "AssetCache.h"
#include "RoadPageProduct.h"
#include "RoadPageProducer.h"
#include <cstddef>
#include <expected>
#include <stop_token>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <unordered_map>

namespace outshine::Generators {
class PreparedRoadPages {
public:
  static constexpr size_t ManifestBytesMost = size_t{8} * 1024u * 1024u;

  [[nodiscard]] std::expected<std::string, std::string>
  StoreManifest(const RoadPageManifest &manifest);
  [[nodiscard]] std::expected<std::optional<std::shared_ptr<const RoadPageManifest>>, std::string>
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
  [[nodiscard]] std::expected<RoadPageLease, std::string>
  Resolve(const RoadPageRequest &request,
          const RoadPageProducer *producer = nullptr,
          std::stop_token stop = {});

private:
  explicit PreparedRoadPages(std::unique_ptr<AssetCache> cache);
  [[nodiscard]] std::expected<bool, std::string>
  ManifestReady(const RoadPageManifest &manifest) const;
  [[nodiscard]] std::expected<std::optional<RoadPageLease>, std::string>
  Read(const std::string &key, size_t bytesMost, const std::string &requestKey = {});
  [[nodiscard]] std::shared_ptr<std::mutex> GenerationGate(const std::string &key);
  std::unique_ptr<AssetCache> Cache_;
  std::mutex Lock_;
  std::unordered_map<std::string, std::weak_ptr<std::mutex>> Generating_;
};
}
#endif
