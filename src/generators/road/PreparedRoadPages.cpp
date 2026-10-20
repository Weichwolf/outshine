#include "PreparedRoadPages.h"
#include "content/AssetGeneration.h"
#include <algorithm>
#include <cstddef>
#include <expected>
#include <filesystem>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <utility>

namespace outshine::Generators {
namespace {
bool FitsHeader(const RoadPageAddress &address, size_t bytesMost) {
  RoadPageAsset empty;
  empty.Address = address;
  empty.Geometry.Parts = address.Parts;
  return EncodeRoadPageAsset(empty, bytesMost).has_value();
}

bool OptionalKey(const std::string &key) {
  return key.empty() || (key.size() == 64 && std::ranges::all_of(key, [](char value) {
                           return (value >= '0' && value <= '9') || (value >= 'a' && value <= 'f');
                         }));
}
}

PreparedRoadPages::PreparedRoadPages(std::unique_ptr<AssetCache> cache)
    : Cache_(std::move(cache)) {}

std::expected<std::unique_ptr<PreparedRoadPages>, std::string>
PreparedRoadPages::Open(const std::string &directory) {
  auto cache = AssetCache::Open((std::filesystem::path(directory) / "assets.sqlite").string());
  if (!cache) { return std::unexpected("could not open the native road page cache"); }
  return std::unique_ptr<PreparedRoadPages>(new PreparedRoadPages(std::move(*cache)));
}

std::expected<std::optional<PreparedRoadPages::Loaded>, std::string>
PreparedRoadPages::Read(const std::string &key, size_t bytesMost) {
  auto cached = Cache_->Load(key, bytesMost);
  if (!cached) { return std::unexpected("could not load the native road page package"); }
  if (!*cached) { return std::optional<Loaded>{}; }
  const auto &record = (**cached).Record();
  if (record.Kind != "road-render-page") {
    return std::unexpected("road page metadata does not match its owning format");
  }
  auto page = DecodeRoadPageAsset((**cached).Bytes(), bytesMost);
  if (!page || RoadPageKey(page->Address) != key || page->Address.Level != record.Level) {
    if (!Cache_->Remove(key)) { return std::unexpected("could not remove an invalid road page"); }
    return std::optional<Loaded>{};
  }
  return Loaded{.Page = std::make_shared<const RoadPageAsset>(std::move(*page)),
                .Record = record,
                .NativeBytes = (**cached).Bytes().size()};
}

std::expected<std::optional<PreparedRoadPages::Loaded>, std::string>
PreparedRoadPages::Load(const RoadPageAddress &address, size_t bytesMost) {
  const auto key = RoadPageKey(address);
  if (key.empty()) { return std::unexpected("invalid road page address"); }
  if (!FitsHeader(address, bytesMost)) {
    return std::unexpected("road page header exceeds its byte budget");
  }
  const std::scoped_lock lock(Lock_);
  return Read(key, bytesMost);
}

std::expected<std::optional<PreparedRoadPages::Loaded>, std::string>
PreparedRoadPages::LoadRequest(const std::string &requestKey, size_t bytesMost) {
  const std::scoped_lock lock(Lock_);
  auto found = Cache_->FindRequest(requestKey);
  if (!found) { return std::unexpected("could not find the native road page demand"); }
  if (!*found) { return std::optional<Loaded>{}; }
  return Read((**found).Key, bytesMost);
}

std::expected<PreparedRoadPages::Loaded, std::string>
PreparedRoadPages::Resolve(const RoadPageAddress &address,
                           const std::string &parent,
                           const std::string &requestKey,
                           size_t bytesMost,
                           const Factory &factory) {
  const auto key = RoadPageKey(address);
  if (key.empty()) { return std::unexpected("invalid road page address"); }
  if (!OptionalKey(parent) || !OptionalKey(requestKey)) {
    return std::unexpected("invalid road page hierarchy or request identity");
  }
  if (!FitsHeader(address, bytesMost)) {
    return std::unexpected("road page header exceeds its byte budget");
  }
  const std::scoped_lock lock(Lock_);
  auto existing = Read(key, bytesMost);
  if (!existing) { return std::unexpected(std::move(existing.error())); }
  if (*existing) {
    if (!requestKey.empty() && (**existing).Record.RequestKey != requestKey) {
      const auto bound = Cache_->BindRequest(key, requestKey);
      if (!bound || !*bound) { return std::unexpected("could not bind the existing road page"); }
    }
    return std::move(**existing);
  }
  auto generated = ResolveAsset(
      *Cache_,
      key,
      bytesMost,
      [&](size_t limit) -> std::expected<GeneratedAssetPackage, std::string> {
        if (!factory) { return std::unexpected("road page miss has no generator"); }
        auto geometry = factory(limit);
        if (!geometry) { return std::unexpected(std::move(geometry.error())); }
        const RoadPageAsset page{.Address = address, .Geometry = std::move(*geometry)};
        auto bytes = EncodeRoadPageAsset(page, limit);
        if (!bytes) { return std::unexpected("road page is invalid or exceeds its byte budget"); }
        const auto bounds = RoadPageBoundsEcef(page);
        return GeneratedAssetPackage{
            .Records = {{.Key = key,
                         .Kind = "road-render-page",
                         .Bounds = bounds,
                         .Package = {},
                         .ByteCount = bytes->size(),
                         .Level = address.Level,
                         .Parent = parent,
                         .RequestKey =
                             requestKey.empty() ? std::nullopt : std::optional(requestKey)}},
            .Bytes = std::move(*bytes)};
      });
  if (!generated) { return std::unexpected(std::move(generated.error())); }
  auto loaded = DecodeRoadPageAsset(generated->Bytes(), bytesMost);
  if (!loaded || RoadPageKey(loaded->Address) != key) {
    return std::unexpected("published road page failed native validation");
  }
  return Loaded{.Page = std::make_shared<const RoadPageAsset>(std::move(*loaded)),
                .Record = generated->Record(),
                .NativeBytes = generated->Bytes().size()};
}
}
