#include "PreparedRoadPages.h"
#include "Sha256.h"
#include <cstddef>
#include <expected>
#include <memory>
#include <mutex>
#include <optional>
#include <span>
#include <string>
#include <stop_token>
#include <vector>
#include <utility>

namespace outshine::Generators {
std::expected<PreparedRoadPages::ManifestWrite, PreparedRoadPages::Failure>
PreparedRoadPages::PrepareManifest(const RoadPageManifest &manifest) {
  auto bytes = EncodeRoadPageManifest(manifest, ManifestBytesMost);
  if (!bytes) {
    return std::unexpected(std::string("road page manifest is invalid or exceeds its byte budget"));
  }
  auto key = Sha256Hex(bytes->data(), bytes->size());
  std::vector<AssetRecord> records{{.Key = key,
                                    .Kind = "road-page-manifest",
                                    .Bounds = RoadPageManifestBounds(manifest),
                                    .Package = {},
                                    .ByteCount = bytes->size(),
                                    .Parent = {}}};
  auto prepared = Cache_->PreparePublication(std::move(records), std::move(*bytes));
  if (!prepared) { return std::unexpected(prepared.error()); }
  std::vector<ManifestWrite::Dependency> dependencies;
  dependencies.reserve(manifest.Pages.size());
  for (const auto &page : manifest.Pages) {
    dependencies.push_back({.Key = page.Key, .ByteCount = page.ByteCount, .Level = page.Level});
  }
  return ManifestWrite(std::move(key), std::move(*prepared), std::move(dependencies));
}

std::expected<std::string, PreparedRoadPages::Failure>
PreparedRoadPages::StoreManifest(ManifestWrite &write, const std::stop_token &stop) {
  if (stop.stop_requested()) {
    return std::unexpected(std::string("road manifest publication canceled"));
  }
  if (!write.DependenciesReady_) {
    while (write.NextDependency_ < write.Dependencies_.size()) {
      if (stop.stop_requested()) {
        return std::unexpected(std::string("road manifest publication canceled"));
      }
      const auto &page = write.Dependencies_[write.NextDependency_];
      const auto record = Cache_->Find(page.Key);
      if (!record) { return std::unexpected(record.error()); }
      if (!*record || (**record).Kind != "road-render-page" ||
          (**record).ByteCount != page.ByteCount || (**record).Level != page.Level) {
        return std::unexpected(std::string("road page manifest has missing native dependencies"));
      }
      ++write.NextDependency_;
    }
    write.DependenciesReady_ = true;
    std::vector<ManifestWrite::Dependency>().swap(write.Dependencies_);
  }
  if (!write.Committed_) {
    if (stop.stop_requested()) {
      return std::unexpected(std::string("road manifest publication canceled"));
    }
    const auto published = Cache_->Publish(write.Package_);
    if (!published) { return std::unexpected(published.error()); }
    write.Committed_ = true;
  }
  auto restored = LoadManifest(write.Key_);
  if (!restored) { return std::unexpected(std::move(restored.error())); }
  if (!*restored) {
    return std::unexpected(std::string("published road manifest failed native validation"));
  }
  if (stop.stop_requested()) {
    return std::unexpected(std::string("road manifest publication canceled"));
  }
  return write.Key_;
}

std::expected<std::optional<std::shared_ptr<const RoadPageManifest>>, PreparedRoadPages::Failure>
PreparedRoadPages::LoadManifest(const std::string &key) {
  auto package = Cache_->Load(key, ManifestBytesMost);
  if (!package) { return std::unexpected(package.error()); }
  if (!*package) { return std::nullopt; }
  if ((**package).Record().Kind != "road-page-manifest") {
    return std::unexpected(std::string("road manifest references a different native product kind"));
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
