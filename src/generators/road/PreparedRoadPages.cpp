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

std::expected<GeneratedAssetPackage, std::string>
EncodePackage(const RoadPageAsset &page, AssetRecord record, size_t bytesMost) {
  auto bytes = EncodeRoadPageAsset(page, bytesMost);
  if (!bytes) { return std::unexpected("road page is invalid or exceeds its byte budget"); }
  record.Bounds = RoadPageBoundsEcef(page);
  record.ByteCount = bytes->size();
  return GeneratedAssetPackage{.Records = {std::move(record)}, .Bytes = std::move(*bytes)};
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
PreparedRoadPages::Read(const std::string &key, size_t bytesMost, const std::string &requestKey) {
  auto cached = Cache_->Load(key, bytesMost);
  if (!cached) { return std::unexpected("could not load the native road page package"); }
  if (!*cached) { return std::optional<Loaded>{}; }
  auto record = (**cached).Record();
  if (record.Kind != "road-render-page") {
    return std::unexpected("road page metadata does not match its owning format");
  }
  auto page = DecodeRoadPageAsset((**cached).Bytes(), bytesMost);
  if (!page || RoadPageKey(page->Address) != key || page->Address.Level != record.Level) {
    if (!Cache_->Remove(key)) { return std::unexpected("could not remove an invalid road page"); }
    return std::optional<Loaded>{};
  }
  if (!requestKey.empty() && record.RequestKey != requestKey) {
    const auto bound = Cache_->BindRequest(key, requestKey);
    if (!bound || !*bound) { return std::unexpected("could not bind the existing road page"); }
    record.RequestKey = requestKey;
  }
  return Loaded{.Page = std::make_shared<const RoadPageAsset>(std::move(*page)),
                .Record = std::move(record),
                .NativeBytes = (**cached).Bytes().size()};
}

std::shared_ptr<std::mutex> PreparedRoadPages::GenerationGate(const std::string &key) {
  const std::scoped_lock lock(Lock_);
  std::erase_if(Generating_, [](const auto &entry) { return entry.second.expired(); });
  auto &pending = Generating_[key];
  auto gate = pending.lock();
  if (!gate) {
    gate = std::make_shared<std::mutex>();
    pending = gate;
  }
  return gate;
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
PreparedRoadPages::LoadKey(const std::string &key, size_t bytesMost) {
  if (key.empty() || !OptionalKey(key)) {
    return std::unexpected("invalid road page product identity");
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
  {
    const std::scoped_lock lock(Lock_);
    auto existing = Read(key, bytesMost, requestKey);
    if (!existing) { return std::unexpected(std::move(existing.error())); }
    if (*existing) { return std::move(**existing); }
  }
  if (!factory) { return std::unexpected("road page miss has no generator"); }
  const auto gate = GenerationGate(key);
  const std::scoped_lock generating(*gate);
  {
    const std::scoped_lock lock(Lock_);
    auto existing = Read(key, bytesMost, requestKey);
    if (!existing) { return std::unexpected(std::move(existing.error())); }
    if (*existing) { return std::move(**existing); }
  }
  auto generated = [&] -> std::expected<CachedAsset, std::string> {
    auto geometry = factory(bytesMost);
    if (!geometry) { return std::unexpected(std::move(geometry.error())); }
    auto package =
        EncodePackage({.Address = address, .Geometry = std::move(*geometry)},
                      {.Key = key,
                       .Kind = "road-render-page",
                       .Bounds = {},
                       .Package = {},
                       .ByteCount = 0,
                       .Level = address.Level,
                       .Parent = parent,
                       .RequestKey = requestKey.empty() ? std::nullopt : std::optional(requestKey)},
                      bytesMost);
    if (!package) { return std::unexpected(std::move(package.error())); }
    const std::scoped_lock lock(Lock_);
    return ResolveAsset(*Cache_, key, bytesMost, [&](size_t) { return std::move(*package); });
  }();
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
