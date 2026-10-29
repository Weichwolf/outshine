#include "PreparedRoadPages.h"
#include "AssetGeneration.h"
#include <algorithm>
#include <cstddef>
#include <expected>
#include <filesystem>
#include <memory>
#include <mutex>
#include <optional>
#include <stop_token>
#include <string>
#include <utility>
#include <variant>

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

std::expected<void, PreparedRoadPages::Failure>
BindRecord(AssetCache &cache, AssetRecord &record, const std::string &requestKey) {
  if (requestKey.empty() || record.RequestKey == requestKey) { return {}; }
  const auto bound = cache.BindRequest(record.Key, requestKey);
  if (!bound) { return std::unexpected(bound.error()); }
  if (!*bound) { return std::unexpected(std::string("could not bind the existing road page")); }
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
  std::optional<RoadPageAsset> page;
  auto cached =
      Cache_->LoadValidated(key, "road-render-page", bytesMost, [&](const CachedAsset &asset) {
        page = DecodeRoadPageAsset(asset.Bytes(), bytesMost);
        return page && RoadPageKey(page->Address) == key &&
               page->Address.Level == asset.Record().Level;
      });
  if (!cached) { return std::unexpected(DescribeFailure(cached.error())); }
  if (!*cached || !page) { return std::optional<RoadPageLease>{}; }
  auto record = (**cached).Record();
  auto bound = BindRecord(*Cache_, record, requestKey);
  if (!bound) { return std::unexpected(DescribeFailure(bound.error())); }
  return RoadPageLease{.Page = std::make_unique<RoadPageAsset>(std::move(*page)),
                       .Record = std::move(record),
                       .NativeBytes = (**cached).Bytes().size()};
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
  auto record = ReadRecord(key, address, bytesMost);
  if (!record) { return std::unexpected(DescribeFailure(record.error())); }
  return std::move(*record);
}

std::expected<std::optional<AssetRecord>, PreparedRoadPages::Failure> PreparedRoadPages::ReadRecord(
    const std::string &key, const RoadPageAddress &address, size_t bytesMost) {
  auto record = Cache_->Find(key);
  if (!record) { return std::unexpected(record.error()); }
  if (!*record) { return std::nullopt; }
  if ((**record).Kind != "road-render-page") {
    return std::unexpected(std::string("road page metadata differs from its owning format"));
  }
  if ((**record).Level != address.Level || (**record).ByteCount == 0) { return std::nullopt; }
  if ((**record).ByteCount > bytesMost) {
    return std::unexpected(std::string("road page metadata exceeds its native payload budget"));
  }
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

std::string PreparedRoadPages::DescribeFailure(const Failure &failure) {
  if (const auto *message = std::get_if<std::string>(&failure)) { return *message; }
  switch (std::get<AssetCacheError>(failure)) {
    case AssetCacheError::InvalidInput: return "invalid road page cache input";
    case AssetCacheError::Storage: return "road page cache storage failed";
    case AssetCacheError::Busy: return "road page cache publication is busy";
    case AssetCacheError::UnsupportedVersion: return "unsupported road page cache version";
    case AssetCacheError::CapacityExceeded: return "road page package exceeds its byte budget";
  }
  return "unknown road page cache failure";
}

std::expected<PreparedRoadPages::PageWrite, PreparedRoadPages::Failure>
PreparedRoadPages::PreparePage(RoadPageRequest request) {
  auto key = RequestKey(request);
  if (!key) { return std::unexpected(std::move(key.error())); }
  return PageWrite(std::move(request), std::move(*key));
}

std::expected<void, PreparedRoadPages::Failure>
PreparedRoadPages::ResolvePageRecord(PageWrite &write) {
  using Phase = PageWrite::Phase;
  const auto &request = write.Request_;
  ++write.Costs_.MetadataReads;
  auto record = ReadRecord(write.Key_, request.Address, request.NativeBytesMost);
  if (!record) { return std::unexpected(std::move(record.error())); }
  if (*record) {
    write.Record_ = std::move(**record);
    if (write.Phase_ == Phase::Restore && !request.RequestKey.empty()) {
      write.Record_.RequestKey = request.RequestKey;
    }
    write.Package_.reset();
    write.Phase_ = Phase::Bind;
  } else if (write.Phase_ == Phase::Restore) {
    return std::unexpected(std::string("published road page metadata is unavailable"));
  } else {
    write.Phase_ = write.Phase_ == Phase::Lookup ? Phase::Generate : Phase::Publish;
  }
  return {};
}

std::expected<void, PreparedRoadPages::Failure> PreparedRoadPages::GeneratePage(
    PageWrite &write, const RoadPageProducer *producer, const std::stop_token &stop) {
  if (producer == nullptr) {
    return std::unexpected(std::string("road page miss has no producer"));
  }
  auto package = ProducePackage(write.Request_, write.Key_, *producer, stop);
  if (!package) { return std::unexpected(std::move(package.error())); }
  ++write.Costs_.Preparations;
  auto prepared =
      Cache_->PreparePublication(std::move(package->Records), std::move(package->Bytes));
  if (!prepared) { return std::unexpected(prepared.error()); }
  write.Package_.emplace(std::move(*prepared));
  write.Phase_ = PageWrite::Phase::Check;
  return {};
}

std::expected<AssetRecord, PreparedRoadPages::Failure> PreparedRoadPages::Prepare(
    PageWrite &write, const RoadPageProducer *producer, const std::stop_token &stop) {
  using Phase = PageWrite::Phase;
  while (true) {
    if (stop.stop_requested()) {
      return std::unexpected(std::string("road page request canceled"));
    }
    switch (write.Phase_) {
      case Phase::Lookup:
      case Phase::Check:
      case Phase::Restore: {
        auto resolved = ResolvePageRecord(write);
        if (!resolved) { return std::unexpected(std::move(resolved.error())); }
        break;
      }
      case Phase::Generate: {
        auto generated = GeneratePage(write, producer, stop);
        if (!generated) { return std::unexpected(std::move(generated.error())); }
        break;
      }
      case Phase::Publish: {
        if (!write.Package_) {
          return std::unexpected(std::string("road publication lost its package"));
        }
        ++write.Costs_.Publications;
        const auto published = Cache_->Publish(*write.Package_);
        if (!published) { return std::unexpected(published.error()); }
        write.Phase_ = Phase::Restore;
        break;
      }
      case Phase::Bind: {
        auto bound = BindRecord(*Cache_, write.Record_, write.Request_.RequestKey);
        if (!bound) { return std::unexpected(std::move(bound.error())); }
        write.Phase_ = Phase::Complete;
        break;
      }
      case Phase::Complete: return write.Record_;
    }
  }
}

std::expected<AssetRecord, std::string> PreparedRoadPages::Prepare(const RoadPageRequest &request,
                                                                   const RoadPageProducer *producer,
                                                                   const std::stop_token &stop) {
  auto write = PreparePage(request);
  if (!write) { return std::unexpected(DescribeFailure(write.error())); }
  auto record = Prepare(*write, producer, stop);
  if (!record) { return std::unexpected(DescribeFailure(record.error())); }
  return std::move(*record);
}

}
