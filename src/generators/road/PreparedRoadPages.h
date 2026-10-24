#ifndef OUTSHINE_GENERATORS_ROAD_PREPAREDROADPAGES_H
#define OUTSHINE_GENERATORS_ROAD_PREPAREDROADPAGES_H

#include "RoadPageAsset.h"
#include "RoadPageManifest.h"
#include "AssetCache.h"
#include <cstddef>
#include <expected>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <unordered_map>

namespace outshine::Generators {
class PreparedRoadPages {
public:
  struct Loaded {
    std::unique_ptr<RoadPageAsset> Page;
    AssetRecord Record;
    size_t NativeBytes = 0;
  };

  static constexpr size_t ManifestBytesMost = size_t{8} * 1024u * 1024u;

  [[nodiscard]] std::expected<std::string, std::string>
  StoreManifest(const RoadPageManifest &manifest);
  [[nodiscard]] std::expected<std::optional<std::shared_ptr<const RoadPageManifest>>, std::string>
  LoadManifest(const std::string &key);
  [[nodiscard]] std::expected<void, std::string> InvalidateManifest(const std::string &key);

  using Factory = std::function<std::expected<RoadPageGeometry, std::string>(size_t)>;

  [[nodiscard]] static std::expected<std::unique_ptr<PreparedRoadPages>, std::string>
  Open(const std::string &directory);
  [[nodiscard]] std::expected<std::optional<AssetRecord>, std::string>
  Inspect(const RoadPageAddress &address, size_t bytesMost);
  [[nodiscard]] std::expected<std::optional<Loaded>, std::string>
  Load(const RoadPageAddress &address, size_t bytesMost);
  [[nodiscard]] std::expected<std::optional<Loaded>, std::string> LoadKey(const std::string &key,
                                                                          size_t bytesMost);
  [[nodiscard]] std::expected<std::optional<Loaded>, std::string>
  LoadRequest(const std::string &requestKey, size_t bytesMost);
  [[nodiscard]] std::expected<Loaded, std::string> Resolve(const RoadPageAddress &address,
                                                           const std::string &parent,
                                                           const std::string &requestKey,
                                                           size_t bytesMost,
                                                           const Factory &factory);

private:
  explicit PreparedRoadPages(std::unique_ptr<AssetCache> cache);
  [[nodiscard]] std::expected<bool, std::string>
  ManifestReady(const RoadPageManifest &manifest) const;
  [[nodiscard]] std::expected<std::optional<Loaded>, std::string>
  Read(const std::string &key, size_t bytesMost, const std::string &requestKey = {});
  [[nodiscard]] std::shared_ptr<std::mutex> GenerationGate(const std::string &key);
  std::unique_ptr<AssetCache> Cache_;
  std::mutex Lock_;
  std::unordered_map<std::string, std::weak_ptr<std::mutex>> Generating_;
};
}
#endif
