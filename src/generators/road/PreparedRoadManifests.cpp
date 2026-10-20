#include "PreparedRoadPages.h"
#include "Sha256.h"
#include <cstddef>
#include <expected>
#include <memory>
#include <mutex>
#include <optional>
#include <span>
#include <string>
#include <utility>

namespace outshine::Generators {
std::expected<bool, std::string>
PreparedRoadPages::ManifestReady(const RoadPageManifest &manifest) const {
  for (const auto &page : manifest.Pages) {
    const auto record = Cache_->Find(page.Key);
    if (!record) { return std::unexpected("could not inspect road manifest dependencies"); }
    if (!*record || (**record).Kind != "road-render-page" ||
        (**record).ByteCount != page.ByteCount || (**record).Level != page.Level) {
      return false;
    }
  }
  return true;
}

std::expected<std::string, std::string>
PreparedRoadPages::StoreManifest(const RoadPageManifest &manifest) {
  const auto bytes = EncodeRoadPageManifest(manifest, ManifestBytesMost);
  if (!bytes) {
    return std::unexpected("road page manifest is invalid or exceeds its byte budget");
  }
  std::string key = Sha256Hex(bytes->data(), bytes->size());
  const std::scoped_lock lock(Lock_);
  const auto ready = ManifestReady(manifest);
  if (!ready) { return std::unexpected(ready.error()); }
  if (!*ready) { return std::unexpected("road page manifest has missing native dependencies"); }
  const AssetRecord record{.Key = key,
                           .Kind = "road-page-manifest",
                           .Bounds = RoadPageManifestBounds(manifest),
                           .Package = {},
                           .ByteCount = bytes->size(),
                           .Parent = {}};
  const auto published = Cache_->Publish(std::span{&record, size_t{1}}, *bytes);
  if (!published) { return std::unexpected("could not publish the native road page manifest"); }
  return key;
}

std::expected<std::optional<std::shared_ptr<const RoadPageManifest>>, std::string>
PreparedRoadPages::LoadManifest(const std::string &key) {
  const std::scoped_lock lock(Lock_);
  auto package = Cache_->Load(key, ManifestBytesMost);
  if (!package) { return std::unexpected("could not load the native road page manifest"); }
  if (!*package) { return std::nullopt; }
  if ((**package).Record().Kind != "road-page-manifest") {
    return std::unexpected("road manifest references a different native product kind");
  }
  const auto bytes = (**package).Bytes();
  auto manifest = DecodeRoadPageManifest(bytes, ManifestBytesMost);
  if (!manifest || Sha256Hex(bytes.data(), bytes.size()) != key) { return std::nullopt; }
  return std::make_shared<const RoadPageManifest>(std::move(*manifest));
}

std::expected<void, std::string> PreparedRoadPages::InvalidateManifest(const std::string &key) {
  if (key.empty()) { return {}; }
  const std::scoped_lock lock(Lock_);
  const auto found = Cache_->Find(key);
  if (!found) { return std::unexpected("could not inspect the incomplete road page manifest"); }
  if (!*found) { return {}; }
  if ((**found).Kind != "road-page-manifest") {
    return std::unexpected("incomplete road pages reference another native product kind");
  }
  const auto removed = Cache_->Remove(key);
  if (!removed) {
    return std::unexpected("could not invalidate the incomplete road page manifest");
  }
  return {};
}

}
