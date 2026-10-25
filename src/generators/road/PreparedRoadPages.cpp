#include "PreparedRoadPages.h"
#include "AssetGeneration.h"
#include <algorithm>
#include <cstddef>
#include <condition_variable>
#include <expected>
#include <filesystem>
#include <memory>
#include <mutex>
#include <optional>
#include <stop_token>
#include <string>
#include <utility>

namespace outshine::Generators {
struct PreparedRoadPages::Generation {
  class Lease {
  public:
    explicit Lease(Generation &gate) noexcept : Gate_(gate) {}

    ~Lease() {
      {
        const std::scoped_lock held(Gate_.Lock);
        Gate_.Busy = false;
      }
      Gate_.Available.notify_all();
    }

    Lease(const Lease &) = delete;
    Lease &operator=(const Lease &) = delete;

  private:
    Generation &Gate_;
  };

  [[nodiscard]] bool Acquire(const std::stop_token &stop) {
    std::unique_lock held(Lock);
    if (!Available.wait(held, stop, [this] { return !Busy; }) || stop.stop_requested()) {
      return false;
    }
    Busy = true;
    return true;
  }

  std::mutex Lock;
  std::condition_variable_any Available;
  bool Busy = false;
};

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

std::expected<std::string, std::string> RequestKey(const RoadPageRequest &request) {
  auto key = RoadPageKey(request.Address);
  if (key.empty()) { return std::unexpected("invalid road page address"); }
  if (!OptionalKey(request.Parent) || !OptionalKey(request.RequestKey)) {
    return std::unexpected("invalid road page hierarchy or request identity");
  }
  if (!FitsHeader(request.Address, request.NativeBytesMost)) {
    return std::unexpected("road page header exceeds its byte budget");
  }
  return key;
}

std::expected<void, std::string>
BindRecord(AssetCache &cache, AssetRecord &record, const std::string &requestKey) {
  if (requestKey.empty()) { return {}; }
  const auto bound = cache.BindRequest(record.Key, requestKey);
  if (!bound || !*bound) { return std::unexpected("could not bind the existing road page"); }
  record.RequestKey = requestKey;
  return {};
}

std::expected<GeneratedAssetPackage, std::string>
EncodePackage(const RoadPageAsset &page, AssetRecord record, size_t bytesMost) {
  auto bytes = EncodeRoadPageAsset(page, bytesMost);
  if (!bytes) { return std::unexpected(std::string(bytes.error())); }
  record.Bounds = RoadPageBoundsEcef(page);
  record.ByteCount = bytes->size();
  return GeneratedAssetPackage{.Records = {std::move(record)}, .Bytes = std::move(*bytes)};
}

std::expected<GeneratedAssetPackage, std::string> ProducePackage(const RoadPageRequest &request,
                                                                 const std::string &key,
                                                                 const RoadPageProducer &producer,
                                                                 const std::stop_token &stop) {
  if (stop.stop_requested()) { return std::unexpected("road page request canceled"); }
  auto geometry = producer.Produce(request, stop);
  if (stop.stop_requested()) { return std::unexpected("road page request canceled"); }
  if (!geometry) { return std::unexpected(std::move(geometry.error())); }
  return EncodePackage(
      {.Address = request.Address, .Geometry = std::move(*geometry)},
      {.Key = key,
       .Kind = "road-render-page",
       .Bounds = {},
       .Package = {},
       .ByteCount = 0,
       .Level = request.Address.Level,
       .Parent = request.Parent,
       .RequestKey = request.RequestKey.empty() ? std::nullopt : std::optional(request.RequestKey)},
      request.NativeBytesMost);
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

std::expected<std::optional<RoadPageLease>, std::string>
PreparedRoadPages::Read(const std::string &key, size_t bytesMost, const std::string &requestKey) {
  auto cached = Cache_->Load(key, bytesMost);
  if (!cached) { return std::unexpected("could not load the native road page package"); }
  if (!*cached) {
    const auto stale = Cache_->Find(key);
    if (!stale) { return std::unexpected("could not inspect the missing road page"); }
    if (*stale && (**stale).Kind == "road-render-page" && !Cache_->Remove(key)) {
      return std::unexpected("could not remove the missing road page metadata");
    }
    return std::optional<RoadPageLease>{};
  }
  auto record = (**cached).Record();
  if (record.Kind != "road-render-page") {
    return std::unexpected("road page metadata does not match its owning format");
  }
  auto page = DecodeRoadPageAsset((**cached).Bytes(), bytesMost);
  if (!page || RoadPageKey(page->Address) != key || page->Address.Level != record.Level) {
    if (!Cache_->Remove(key)) { return std::unexpected("could not remove an invalid road page"); }
    return std::optional<RoadPageLease>{};
  }
  auto bound = BindRecord(*Cache_, record, requestKey);
  if (!bound) { return std::unexpected(std::move(bound.error())); }
  return RoadPageLease{.Page = std::make_unique<RoadPageAsset>(std::move(*page)),
                       .Record = std::move(record),
                       .NativeBytes = (**cached).Bytes().size()};
}

std::shared_ptr<PreparedRoadPages::Generation>
PreparedRoadPages::GenerationGate(const std::string &key) {
  const std::scoped_lock lock(Lock_);
  std::erase_if(Generating_, [](const auto &entry) { return entry.second.expired(); });
  auto &pending = Generating_[key];
  auto gate = pending.lock();
  if (!gate) {
    gate = std::make_shared<Generation>();
    pending = gate;
  }
  return gate;
}

std::expected<std::optional<RoadPageLease>, std::string>
PreparedRoadPages::Load(const RoadPageAddress &address, size_t bytesMost) {
  const auto key = RoadPageKey(address);
  if (key.empty()) { return std::unexpected("invalid road page address"); }
  if (!FitsHeader(address, bytesMost)) {
    return std::unexpected("road page header exceeds its byte budget");
  }
  const std::scoped_lock lock(Lock_);
  return Read(key, bytesMost);
}

std::expected<std::optional<AssetRecord>, std::string>
PreparedRoadPages::Inspect(const RoadPageAddress &address, size_t bytesMost) {
  const auto key = RoadPageKey(address);
  if (key.empty() || !FitsHeader(address, bytesMost)) {
    return std::unexpected("road page metadata needs a valid bounded address");
  }
  const std::scoped_lock lock(Lock_);
  return ReadRecord(key, address.Level, bytesMost);
}

std::expected<std::optional<AssetRecord>, std::string> PreparedRoadPages::ReadRecord(
    const std::string &key, uint32_t level, size_t bytesMost, const std::string &requestKey) {
  auto record = Cache_->Find(key);
  if (!record) { return std::unexpected("could not inspect the native road page metadata"); }
  if (!*record) { return std::nullopt; }
  if ((**record).Kind != "road-render-page") {
    return std::unexpected("road page metadata differs from its owning format");
  }
  if ((**record).Level != level || (**record).ByteCount == 0) {
    if (!Cache_->Remove(key)) {
      return std::unexpected("could not remove invalid road page metadata");
    }
    return std::nullopt;
  }
  if ((**record).ByteCount > bytesMost) {
    return std::unexpected("road page metadata exceeds its native payload budget");
  }
  auto bound = BindRecord(*Cache_, **record, requestKey);
  if (!bound) { return std::unexpected(std::move(bound.error())); }
  return std::move(*record);
}

std::expected<std::optional<RoadPageLease>, std::string>
PreparedRoadPages::LoadKey(const std::string &key, size_t bytesMost) {
  if (key.empty() || !OptionalKey(key)) {
    return std::unexpected("invalid road page product identity");
  }
  const std::scoped_lock lock(Lock_);
  return Read(key, bytesMost);
}

std::expected<std::optional<RoadPageLease>, std::string>
PreparedRoadPages::LoadRequest(const std::string &requestKey, size_t bytesMost) {
  const std::scoped_lock lock(Lock_);
  auto found = Cache_->FindRequest(requestKey);
  if (!found) { return std::unexpected("could not find the native road page demand"); }
  if (!*found) { return std::optional<RoadPageLease>{}; }
  return Read((**found).Key, bytesMost, requestKey);
}

std::expected<AssetRecord, std::string> PreparedRoadPages::Prepare(const RoadPageRequest &request,
                                                                   const RoadPageProducer *producer,
                                                                   std::stop_token stop) {
  if (stop.stop_requested()) { return std::unexpected("road page request canceled"); }
  auto key = RequestKey(request);
  if (!key) { return std::unexpected(std::move(key.error())); }
  const auto lookup = [&] {
    const std::scoped_lock lock(Lock_);
    return ReadRecord(*key, request.Address.Level, request.NativeBytesMost, request.RequestKey);
  };
  auto existing = lookup();
  if (!existing) { return std::unexpected(std::move(existing.error())); }
  if (*existing) { return std::move(**existing); }
  if (producer == nullptr) { return std::unexpected("road page miss has no producer"); }
  const auto gate = GenerationGate(*key);
  if (!gate->Acquire(stop)) { return std::unexpected("road page request canceled"); }
  const Generation::Lease generating(*gate);
  existing = lookup();
  if (!existing) { return std::unexpected(std::move(existing.error())); }
  if (*existing) { return std::move(**existing); }
  auto package = ProducePackage(request, *key, *producer, stop);
  if (!package) { return std::unexpected(std::move(package.error())); }
  const std::scoped_lock lock(Lock_);
  existing = ReadRecord(*key, request.Address.Level, request.NativeBytesMost, request.RequestKey);
  if (!existing) { return std::unexpected(std::move(existing.error())); }
  if (*existing) { return std::move(**existing); }
  auto committed = CommitAsset(*Cache_, *key, request.NativeBytesMost, *package);
  if (!committed) { return std::unexpected(std::move(committed.error())); }
  if (!request.RequestKey.empty()) { committed->RequestKey = request.RequestKey; }
  return std::move(*committed);
}

std::expected<RoadPageLease, std::string> PreparedRoadPages::Resolve(
    const RoadPageRequest &request, const RoadPageProducer *producer, std::stop_token stop) {
  if (stop.stop_requested()) { return std::unexpected("road page request canceled"); }
  auto key = RequestKey(request);
  if (!key) { return std::unexpected(std::move(key.error())); }
  const auto &requestKey = request.RequestKey;
  const auto bytesMost = request.NativeBytesMost;
  {
    const std::scoped_lock lock(Lock_);
    auto existing = Read(*key, bytesMost, requestKey);
    if (!existing) { return std::unexpected(std::move(existing.error())); }
    if (*existing) { return std::move(**existing); }
  }
  if (producer == nullptr) { return std::unexpected("road page miss has no producer"); }
  const auto gate = GenerationGate(*key);
  if (!gate->Acquire(stop)) { return std::unexpected("road page request canceled"); }
  const Generation::Lease generating(*gate);
  {
    const std::scoped_lock lock(Lock_);
    auto existing = Read(*key, bytesMost, requestKey);
    if (!existing) { return std::unexpected(std::move(existing.error())); }
    if (*existing) { return std::move(**existing); }
  }
  auto package = ProducePackage(request, *key, *producer, stop);
  if (!package) { return std::unexpected(std::move(package.error())); }
  auto generated = [&] {
    const std::scoped_lock lock(Lock_);
    return ResolveAsset(*Cache_, *key, bytesMost, [&](size_t) { return std::move(*package); });
  }();
  if (!generated) { return std::unexpected(std::move(generated.error())); }
  auto loaded = DecodeRoadPageAsset(generated->Bytes(), bytesMost);
  if (!loaded || RoadPageKey(loaded->Address) != *key) {
    return std::unexpected("published road page failed native validation");
  }
  return RoadPageLease{.Page = std::make_unique<RoadPageAsset>(std::move(*loaded)),
                       .Record = generated->Record(),
                       .NativeBytes = generated->Bytes().size()};
}
}
