#ifndef OUTSHINE_GENERATORS_ROAD_PREPAREDROADPAGES_H
#define OUTSHINE_GENERATORS_ROAD_PREPAREDROADPAGES_H

#include "RoadPageAsset.h"
#include "content/AssetCache.h"
#include <cstddef>
#include <expected>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <string>

namespace outshine::Generators {
class PreparedRoadPages {
public:
  struct Loaded {
    std::shared_ptr<const RoadPageAsset> Page;
    AssetRecord Record;
    size_t NativeBytes = 0;
  };

  using Factory = std::function<std::expected<RoadPageGeometry, std::string>(size_t)>;

  [[nodiscard]] static std::expected<std::unique_ptr<PreparedRoadPages>, std::string>
  Open(const std::string &directory);
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
  [[nodiscard]] std::expected<std::optional<Loaded>, std::string> Read(const std::string &key,
                                                                       size_t bytesMost);
  std::unique_ptr<AssetCache> Cache_;
  std::mutex Lock_;
};
}
#endif
