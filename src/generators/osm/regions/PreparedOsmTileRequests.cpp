#include "PreparedOsmTiles.h"
#include "Sha256.h"
#include <algorithm>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <memory>
#include <optional>
#include <ratio>
#include <stop_token>
#include <string>
#include <utility>

namespace outshine::Generators::Osm {
std::expected<AssetPipeline::LogicalStep, std::string>
PreparedOsmTiles::Resolve(const std::shared_ptr<Production> &work, const std::stop_token &stop) {
  if (stop.stop_requested()) { return std::unexpected("native OSM preparation cancelled"); }
  switch (work->Stage) {
    case Production::Phase::Index: return Lookup(*work);
    case Production::Phase::Hit: return LookupHit(*work);
    case Production::Phase::Source: return OpenSource(work);
    case Production::Phase::AwaitSource: return PollSource(work);
    default: return std::unexpected("invalid native OSM request stage");
  }
}

std::expected<AssetPipeline::LogicalStep, std::string> PreparedOsmTiles::Lookup(Production &work) {
  auto record = Cache_->FindRequest(work.Request);
  if (!record) {
    if (record.error() == AssetCacheError::Busy) { return std::optional<AssetProduct>{}; }
    return std::unexpected("native OSM tile index failed");
  }
  if (*record && (**record).Kind != "osm-tile") {
    const auto unbound = Cache_->UnbindRequestIfMatches(**record);
    if (!unbound && unbound.error() != AssetCacheError::Busy) {
      return std::unexpected("native OSM request kind repair failed");
    }
    return std::optional<AssetProduct>{};
  }
  if (*record) {
    const auto bytes = Cache_->PackageBytes((**record).Key, kPackageBytesMost);
    if (!bytes) {
      if (bytes.error() == AssetCacheError::Busy) { return std::optional<AssetProduct>{}; }
      return std::unexpected("native OSM package metadata failed");
    }
    if (!*bytes) { return std::optional<AssetProduct>{}; }
    work.LoadMost = std::max(size_t{1}, **bytes);
  }
  work.Stage = *record ? Production::Phase::Hit : Production::Phase::Source;
  if (!*record) { ++Misses_; }
  return AssetPipeline::InputAdmission{.Bytes = *record ? 3 * work.LoadMost : 0};
}

std::expected<AssetPipeline::LogicalStep, std::string>
PreparedOsmTiles::LookupHit(Production &work) {
  const auto began = std::chrono::steady_clock::now();
  auto loaded = Load(work.Request, work.LoadMost);
  ReadMs_.fetch_add(
      std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - began).count());
  if (!loaded) {
    if (loaded.error() == AssetCacheError::CapacityExceeded) {
      work.Stage = Production::Phase::Index;
      return AssetPipeline::InputAdmission{.Bytes = 0};
    }
    if (loaded.error() == AssetCacheError::Busy) { return std::optional<AssetProduct>{}; }
    return std::unexpected("native OSM tile lookup failed");
  }
  if (*loaded) {
    ++Hits_;
    ReadBytes_ += (**loaded).Bytes().size();
    AssetProduct product(std::move(**loaded));
    product.SetRequestBinding(work.Request);
    return std::optional<AssetProduct>(std::move(product));
  }
  ++Misses_;
  work.Stage = Production::Phase::Source;
  return AssetPipeline::InputAdmission{.Bytes = 0};
}

std::expected<AssetPipeline::LogicalStep, std::string>
PreparedOsmTiles::OpenSource(const std::shared_ptr<Production> &work) {
  if (PublicationBytes_.load() >= Budget_.PublicationBytesMost) {
    if (!work->SourceAdmitted) { return std::optional<AssetProduct>{}; }
    work->SourceAdmitted = false;
    return AssetPipeline::InputAdmission{.Bytes = 0};
  }
  if (!work->SourceAdmitted) {
    work->SourceAdmitted = true;
    return AssetPipeline::InputAdmission{.Bytes = kColdBytesMost};
  }
  auto read = Data::SourceSession::Request(
      Sources_,
      Data::SourceRequest(Data::DataKind::VectorMap,
                          Data::Address::At({.Zoom = work->Definition.Zoom,
                                             .X = static_cast<uint32_t>(work->At.X),
                                             .Y = static_cast<uint32_t>(work->At.Y)})),
      Transport_);
  if (!read) { return std::unexpected(std::move(read.error())); }
  work->Input.emplace(std::move(*read));
  work->Stage = Production::Phase::AwaitSource;
  return PollSource(work);
}

std::expected<AssetPipeline::LogicalStep, std::string>
PreparedOsmTiles::PollSource(const std::shared_ptr<Production> &work) {
  if (!work->Input) { return std::unexpected("native OSM source stage has no read"); }
  auto delivery = work->Input->Poll();
  if (delivery.Where() == Data::SourceDelivery::State::Pending) {
    return std::optional<AssetProduct>{};
  }
  work->Input.reset();
  switch (delivery.Where()) {
    case Data::SourceDelivery::State::Vacant:
    case Data::SourceDelivery::State::Undeclared: return std::unexpected(std::string(kAbsent));
    case Data::SourceDelivery::State::Refused: return std::unexpected(std::string(kRefused));
    case Data::SourceDelivery::State::Consumed:
    case Data::SourceDelivery::State::Pending:
      return std::unexpected("native OSM source reader returned no payload");
    case Data::SourceDelivery::State::Delivered: break;
  }
  auto landing = delivery.Take();
  if (!landing || landing->Bytes.capacity() > kPackageBytesMost) {
    return std::unexpected("invalid or oversized native OSM source payload");
  }
  work->ProductKey = ProductKey(work->Request,
                                Sha256Hex(landing->Bytes.data(), landing->Bytes.size()),
                                landing->SourceId,
                                landing->SourceRevision);
  const auto key = AssetProductKey::Parse(work->ProductKey);
  if (!key) { return std::unexpected("invalid native OSM content identity"); }
  work->Landing = std::move(landing);
  work->Stage = Production::Phase::Generate;
  return AssetPipeline::ResolvedWork{
      .Key = *key,
      .Produce = [this, work](const std::stop_token &next) { return Produce(*work, next); }};
}
}
